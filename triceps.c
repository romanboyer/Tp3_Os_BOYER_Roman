/* triceps.c : l'interpréteur de commande modifié pour devenir biceps */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "creme.h" /* Inclusion de notre librairie réseau */

int run = 1, debug=0;

#define MAXPAR 10 /* nombre maximum de mots dans la commande */

char * Mots[MAXPAR];
int NMots;

void Sortie(void) { run = 0; }

void executeCommande(void)
{
   if (strcmp(Mots[0],"exit") == 0) {
       /* Par sécurité, on arrête le serveur si on quitte l'interpréteur */
       beuip_stop();
       return Sortie();
   }

   /* --- Commandes BEUIP --- */
   if (strcmp(Mots[0], "beuip") == 0) {
       if (NMots == 3 && strcmp(Mots[1], "start") == 0) {
           beuip_start(Mots[2]); /* beuip start pseudo */
           return;
       } else if (NMots == 2 && strcmp(Mots[1], "stop") == 0) {
           beuip_stop(); /* beuip stop */
           return;
       }
       fprintf(stderr, "Utilisation: beuip start <pseudo> | beuip stop\n");
       return;
   }

   /* --- Commandes MESS --- */
   if (strcmp(Mots[0], "mess") == 0) {
       if (NMots == 2 && strcmp(Mots[1], "liste") == 0) {
           mess_liste(); /* afficher la liste des pseudos présents */
           return;
       } else if (NMots >= 3 && strcmp(Mots[1], "all") == 0) {
           /* Reconstitution du message complet pour envoyer à tout le monde */
           char msg[512] = "";
           for(int i = 2; i < NMots; i++) {
               strcat(msg, Mots[i]);
               if (i < NMots - 1) strcat(msg, " ");
           }
           mess_send_all(msg);
           return;
       } else if (NMots >= 3) {
           /* Reconstitution du message complet pour envoyer à un pseudo (Mots[1]) */
           char msg[512] = "";
           for(int i = 2; i < NMots; i++) {
               strcat(msg, Mots[i]);
               if (i < NMots - 1) strcat(msg, " ");
           }
           mess_send_one(Mots[1], msg);
           return;
       }
       fprintf(stderr, "Utilisation: mess liste | mess all <message> | mess <pseudo> <message>\n");
       return;
   }

   fprintf(stderr,"%s : commande inexistante !\n",Mots[0]);
   return;
}

int traiteCommande(char * b) /* separe la ligne de commande en mots */
{
char *d, *f;
int mode =1;
   d=b;
   f=b+strlen(b);
   NMots=0;
   while (d<f) {
     if (mode) { /* on cherche le 1er car. non separateur */
        if ((*d != ' ') && (*d != '\t')) {
           if (NMots == MAXPAR) break;
           Mots[NMots++] = d;
           mode = 0;
        }
     } else { /* on cherche le 1er separateur */
        if ((*d == ' ') || (*d == '\t')) {
          *d = '\0';
          mode = 1;
        }
     } 
     d++;
   }
   if (debug) printf("Il y a %d mots\n",NMots);
   return NMots;
}

int main(int N, char * P[])
{
char *buf=NULL;
size_t lb=0;
int n, i;
   if (N > 2) {
      fprintf(stderr,"Utilisation : %s [-d]\n",P[0]); return 1;
   }
   if (N == 2) {
      if (strcmp(P[1],"-d") == 0) debug=1;
      else fprintf(stderr,"parametre %s invalide !\n",P[1]);
   }
   while (run) {
     printf("biceps> "); /* Changement du prompt triceps> en biceps> */
     if ((n = getline(&buf, &lb, stdin)) != -1) {
         if (buf[n-1] == '\n') buf[--n] = '\0';
         if (debug) printf("Lu %d car.: %s\n",n,buf);
     } else break;
     /* traitement de la commande */
     n = traiteCommande(buf);
     if (debug) {
        printf("La commande est %s !\n",Mots[0]);
        if (n > 1) {
           printf("Les parametres sont :\n");
           for (i=1; i<n; i++) printf("\t%s\n",Mots[i]);
        }
     }
     if (n>0) executeCommande();
     free(buf);
     n=0;
     buf=NULL;
   }
   printf("Bye !\n");
   return 0;
}