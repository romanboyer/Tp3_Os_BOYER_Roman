/* triceps.c : l'interpréteur de commande modifié pour devenir biceps */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "creme.h" 

int run = 1, debug = 0;

#define MAXPAR 10 

char * Mots[MAXPAR];
int NMots;

void executeCommande(void)
{
    if (strcmp(Mots[0], "exit") == 0) {
        run = 0;
        return;
    }

    if (strcmp(Mots[0], "beuip") == 0) {
        if (NMots == 3 && strcmp(Mots[1], "start") == 0) {
            beuip_start(Mots[2]);
            return;
        } else if (NMots == 2 && strcmp(Mots[1], "stop") == 0) {
            beuip_stop();
            return;
        } else if (NMots == 2 && strcmp(Mots[1], "list") == 0) {
            beuip_list();
            return;
        } else if (NMots >= 4 && strcmp(Mots[1], "message") == 0) {
            char msg[512] = "";
            for (int i = 3; i < NMots; i++) {
                strcat(msg, Mots[i]);
                if (i < NMots - 1) strcat(msg, " ");
            }

            if (strcmp(Mots[2], "all") == 0) {
                beuip_message_all(msg);
            } else {
                beuip_message_one(Mots[2], msg);
            }
            return;
        } else if (NMots == 3 && strcmp(Mots[1], "ls") == 0) {
            beuip_ls(Mots[2]);
            return;
        } else if (NMots == 4 && strcmp(Mots[1], "get") == 0) {
            beuip_get(Mots[2], Mots[3]);
            return;
        }
        fprintf(stderr, "Utilisation: beuip start <user> | beuip stop | beuip list | beuip message <user> <msg> | beuip message all <msg>\n");
        return;
    }

    fprintf(stderr, "%s : commande inexistante !\n", Mots[0]);
}

int traiteCommande(char * b) 
{
    char *d, *f;
    int mode = 1;
    d = b;
    f = b + strlen(b);
    NMots = 0;
    while (d < f) {
        if (mode) { 
            if ((*d != ' ') && (*d != '\t')) {
                if (NMots == MAXPAR) break;
                Mots[NMots++] = d;
                mode = 0;
            }
        } else { 
            if ((*d == ' ') || (*d == '\t')) {
                *d = '\0';
                mode = 1;
            }
        } 
        d++;
    }
    if (debug) printf("Il y a %d mots\n", NMots);
    return NMots;
}

int main(int N, char * P[])
{
    char *buf = NULL;
    size_t lb = 0;
    int n;

    if (N > 2) {
        fprintf(stderr,"Utilisation : %s [-d]\n", P[0]); return 1;
    }
    if (N == 2) {
        if (strcmp(P[1], "-d") == 0) debug = 1;
        else fprintf(stderr, "parametre %s invalide !\n", P[1]);
    }

    while (run) {
        printf("biceps> "); 
        if ((n = getline(&buf, &lb, stdin)) != -1) {
            if (n > 0 && buf[n-1] == '\n') buf[--n] = '\0';
            if (debug) printf("Lu %d car.: %s\n", n, buf);
        } else {
            /* Capture du CTRL+D ou d'une erreur d'entrée */
            run = 0;
            break;
        }

        n = traiteCommande(buf);
        if (debug) {
            printf("La commande est %s !\n", Mots[0]);
            if (n > 1) {
                printf("Les parametres sont :\n");
                for (int i = 1; i < n; i++) printf("\t%s\n", Mots[i]);
            }
        }
        if (n > 0) executeCommande();
    }
    
    printf("Bye !\n");
    
    /* Assure la libération de la mémoire et l'arrêt des threads en cas de fermeture inattendue (CTRL+D) */
    beuip_stop();
    if (buf) free(buf);
    
    return 0;
}
