#include "creme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>

#define PORT 9998
#define LBUF 512

/* --- NOUVELLE DONNÉE PARTAGÉE : LA LISTE CHAÎNÉE --- */
#define LPSEUDO 23

struct elt {
    char nom[LPSEUDO+1];
    char adip[16];
    struct elt * next;
};

// Pointeur vers le début de notre liste
static struct elt *liste_contacts = NULL;

// Toujours notre mutex pour éviter les crashs entre l'interpréteur et le serveur
static pthread_mutex_t mutex_table = PTHREAD_MUTEX_INITIALIZER;

static pthread_t serveur_thread;
static int serveur_actif = 0;
static int sid_global = -1;
static char pseudo_global[50];


/* --- FONCTIONS DE GESTION DE LA LISTE --- */
// Attention : ces fonctions doivent être appelées en ayant déjà verrouillé le mutex !

void ajouteElt(char * pseudo, char * adip) {
    struct elt *nouveau, *courant, *precedent;

    // 1. On vérifie si l'IP n'est pas déjà dans la liste (pour éviter les doublons)
    courant = liste_contacts;
    while(courant != NULL) {
        if(strcmp(courant->adip, adip) == 0 && strcmp(courant->nom, pseudo) == 0) {
            return; // Il est déjà là, on ne fait rien
        }
        courant = courant->next;
    }

    // 2. On alloue de la mémoire pour le petit nouveau
    nouveau = malloc(sizeof(struct elt));
    if (!nouveau) {
        perror("Erreur malloc ajouteElt");
        return;
    }
    strncpy(nouveau->nom, pseudo, LPSEUDO);
    nouveau->nom[LPSEUDO] = '\0'; // Sécurité
    strncpy(nouveau->adip, adip, 15);
    nouveau->adip[15] = '\0';
    nouveau->next = NULL;

    // 3. Insertion par ordre alphabétique croissant sur le pseudo
    courant = liste_contacts;
    precedent = NULL;

    while (courant != NULL && strcmp(courant->nom, pseudo) < 0) {
        precedent = courant;
        courant = courant->next;
    }

    if (precedent == NULL) {
        // Insertion en tête de liste
        nouveau->next = liste_contacts;
        liste_contacts = nouveau;
    } else {
        // Insertion au milieu ou à la fin
        nouveau->next = courant;
        precedent->next = nouveau;
    }
}

void supprimeElt(char * adip) {
    struct elt *courant = liste_contacts;
    struct elt *precedent = NULL;

    while (courant != NULL) {
        if (strcmp(courant->adip, adip) == 0) {
            // On le décroche de la liste
            if (precedent == NULL) {
                liste_contacts = courant->next;
            } else {
                precedent->next = courant->next;
            }
            printf("\n[Info] L'utilisateur %s a quitté le réseau.\nbiceps> ", courant->nom);
            fflush(stdout);
            
            free(courant); // On n'oublie pas de libérer la mémoire !
            return; 
        }
        precedent = courant;
        courant = courant->next;
    }
}

void listeElts(void) {
    struct elt *courant = liste_contacts;
    int count = 0;
    
    printf("\n--- Liste des connectés ---\n");
    while (courant != NULL) {
        printf(" - %s (%s)\n", courant->nom, courant->adip);
        courant = courant->next;
        count++;
    }
    if (count == 0) {
        printf(" (Personne pour le moment...)\n");
    }
    printf("---------------------------\n");
}

/* --- FONCTION DE BROADCAST DYNAMIQUE (Étape 2.1) --- */
static void envoyer_broadcast_dynamique(int sid, const char *msg) {
    struct ifaddrs *ifaddr, *ifa;
    char bcast[NI_MAXHOST];
    char adip[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) return;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), adip, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (strcmp(adip, "127.0.0.1") != 0 && ifa->ifa_broadaddr != NULL) {
                if (getnameinfo(ifa->ifa_broadaddr, sizeof(struct sockaddr_in), bcast, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0) {
                    struct sockaddr_in dest;
                    memset(&dest, 0, sizeof(dest));
                    dest.sin_family = AF_INET;
                    dest.sin_port = htons(PORT);
                    dest.sin_addr.s_addr = inet_addr(bcast);
                    sendto(sid, msg, strlen(msg), 0, (struct sockaddr *)&dest, sizeof(dest));
                }
            }
        }
    }
    freeifaddrs(ifaddr);
}

/* --- FONCTION COMMANDE --- */
void commande(char octet1, char *message, char *pseudo) {
    if (!serveur_actif) {
        printf("Erreur : le serveur UDP n'est pas actif.\n");
        return;
    }

    if (octet1 == '3') {
        pthread_mutex_lock(&mutex_table);
        listeElts(); // Appel direct de notre nouvelle fonction
        pthread_mutex_unlock(&mutex_table);

    } else if (octet1 == '4' && pseudo && message) {
        char dest_ip[16] = "";
        int found = 0;

        // Recherche du pseudo dans la liste chaînée
        pthread_mutex_lock(&mutex_table);
        struct elt *courant = liste_contacts;
        while (courant != NULL) {
            if (strcmp(courant->nom, pseudo) == 0) {
                strcpy(dest_ip, courant->adip);
                found = 1;
                break;
            }
            courant = courant->next;
        }
        pthread_mutex_unlock(&mutex_table);

        if (found) {
            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(PORT);
            dest.sin_addr.s_addr = inet_addr(dest_ip);

            char msg9[LBUF + 16];
            snprintf(msg9, sizeof(msg9), "9BEUIP%s", message);
            sendto(sid_global, msg9, strlen(msg9), 0, (struct sockaddr *)&dest, sizeof(dest));
        } else {
            printf("Erreur : Pseudo '%s' introuvable.\n", pseudo);
        }

    } else if (octet1 == '5' && message) {
        char msg9[LBUF + 16];
        snprintf(msg9, sizeof(msg9), "9BEUIP%s", message);

        // Envoi à tous les membres de la liste
        pthread_mutex_lock(&mutex_table);
        struct elt *courant = liste_contacts;
        while (courant != NULL) {
            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(PORT);
            dest.sin_addr.s_addr = inet_addr(courant->adip);
            sendto(sid_global, msg9, strlen(msg9), 0, (struct sockaddr *)&dest, sizeof(dest));
            courant = courant->next;
        }
        pthread_mutex_unlock(&mutex_table);

    } else if (octet1 == '0') {
        char msg0[LBUF + 16];
        snprintf(msg0, sizeof(msg0), "0BEUIP%s", pseudo_global);

        pthread_mutex_lock(&mutex_table);
        struct elt *courant = liste_contacts;
        while (courant != NULL) {
            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(PORT);
            dest.sin_addr.s_addr = inet_addr(courant->adip);
            sendto(sid_global, msg0, strlen(msg0), 0, (struct sockaddr *)&dest, sizeof(dest));
            courant = courant->next;
        }
        pthread_mutex_unlock(&mutex_table);
    }
}

/* --- LE THREAD SERVEUR UDP --- */
void * serveur_udp(void * p) {
    int n;
    struct sockaddr_in SockConf, client_addr;
    socklen_t ls;
    char buf[LBUF];
    int opt = 1;
    char *my_pseudo = (char *)p;

    if ((sid_global = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) pthread_exit(NULL);
    if (setsockopt(sid_global, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) pthread_exit(NULL);

    memset(&SockConf, 0, sizeof(SockConf));
    SockConf.sin_family = AF_INET;
    SockConf.sin_port = htons(PORT);
    SockConf.sin_addr.s_addr = INADDR_ANY;

    if (bind(sid_global, (struct sockaddr *)&SockConf, sizeof(SockConf)) < 0) pthread_exit(NULL);

    char init_msg[LBUF + 16];
    snprintf(init_msg, sizeof(init_msg), "1BEUIP%s", my_pseudo);
    envoyer_broadcast_dynamique(sid_global, init_msg);

    while (serveur_actif) {
        ls = sizeof(client_addr);
        memset(buf, 0, LBUF);
        
        if ((n = recvfrom(sid_global, buf, LBUF - 1, 0, (struct sockaddr *)&client_addr, &ls)) < 0) {
            continue;
        }
        buf[n] = '\0';

        if (strncmp(buf, "WAKEUP", 6) == 0) continue;

        if (n >= 6 && strncmp(&buf[1], "BEUIP", 5) == 0) {
            char code = buf[0];
            char *data = &buf[6];

            if (code == '1' || code == '2') {
                pthread_mutex_lock(&mutex_table);
                // On passe l'IP convertie en string à notre fonction ajouteElt
                ajouteElt(data, inet_ntoa(client_addr.sin_addr));
                pthread_mutex_unlock(&mutex_table);

                if (code == '1') {
                    char ack[LBUF + 16];
                    snprintf(ack, sizeof(ack), "2BEUIP%s", my_pseudo);
                    sendto(sid_global, ack, strlen(ack), 0, (struct sockaddr *)&client_addr, ls);
                }

            } else if (code == '0') {
                pthread_mutex_lock(&mutex_table);
                // On vire le mec qui a cette IP de la liste
                supprimeElt(inet_ntoa(client_addr.sin_addr));
                pthread_mutex_unlock(&mutex_table);

            } else if (code == '9') {
                int found = 0;
                char sender_ip[16];
                strcpy(sender_ip, inet_ntoa(client_addr.sin_addr));

                pthread_mutex_lock(&mutex_table);
                struct elt *courant = liste_contacts;
                while (courant != NULL) {
                    if (strcmp(courant->adip, sender_ip) == 0) {
                        printf("\n[Message de %s] : %s\nbiceps> ", courant->nom, data);
                        fflush(stdout);
                        found = 1;
                        break;
                    }
                    courant = courant->next;
                }
                pthread_mutex_unlock(&mutex_table);
                
                if (!found) {
                    printf("\n[Message inconnu (%s)] : %s\nbiceps> ", sender_ip, data);
                    fflush(stdout);
                }

            } else if (code == '3' || code == '4' || code == '5') {
                fprintf(stderr, "\n[Alerte Sécurité] Tentative d'intrusion (code %c) de %s.\nbiceps> ", 
                        code, inet_ntoa(client_addr.sin_addr));
                fflush(stdout);
            }
        }
    }
    
    close(sid_global);
    sid_global = -1;
    pthread_exit(NULL);
}

/* --- COMMANDES INTERNES BICEPS --- */

int beuip_start(const char *pseudo) {
    if (serveur_actif) {
        printf("Serveur déjà lancé !\n");
        return -1;
    }

    strncpy(pseudo_global, pseudo, 49);
    pseudo_global[49] = '\0';
    serveur_actif = 1;
    
    // Au lancement, on s'assure que la liste est bien vide
    liste_contacts = NULL;

    if (pthread_create(&serveur_thread, NULL, serveur_udp, pseudo_global) != 0) {
        perror("Erreur pthread_create");
        serveur_actif = 0;
        return -1;
    }

    return 0;
}

int beuip_stop(void) {
    if (!serveur_actif) return -1;

    commande('0', NULL, NULL);
    serveur_actif = 0; 

    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(PORT);
    local.sin_addr.s_addr = inet_addr("127.0.0.1");
    sendto(sid_global, "WAKEUP", 6, 0, (struct sockaddr *)&local, sizeof(local));

    pthread_join(serveur_thread, NULL);

    // On vide proprement la liste chaînée avant de quitter pour ne pas avoir de fuite mémoire
    pthread_mutex_lock(&mutex_table);
    struct elt *courant = liste_contacts;
    struct elt *suivant;
    while (courant != NULL) {
        suivant = courant->next;
        free(courant);
        courant = suivant;
    }
    liste_contacts = NULL;
    pthread_mutex_unlock(&mutex_table);

    printf("Serveur UDP stoppé et liste nettoyée.\n");
    return 0;
}

int mess_liste(void) { 
    commande('3', NULL, NULL); 
    return 0; 
}

int mess_send_one(const char *pseudo, const char *message) { 
    commande('4', (char *)message, (char *)pseudo); 
    return 0; 
}

int mess_send_all(const char *message) { 
    commande('5', (char *)message, NULL); 
    return 0; 
}