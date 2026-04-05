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
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT 9998
#define LBUF 512
#define LPSEUDO 23

struct elt {
    char nom[LPSEUDO+1];
    char adip[16];
    struct elt * next;
};

static struct elt *liste_contacts = NULL;
static pthread_mutex_t mutex_table = PTHREAD_MUTEX_INITIALIZER;

static pthread_t serveur_thread;
static pthread_t serveur_tcp_thread; // Nouveau thread pour TCP

static int serveur_actif = 0;
static int sid_global = -1;
static int sid_tcp_global = -1; // Socket global TCP
static char pseudo_global[50];


/* --- GESTION LISTE CHAINEE --- */

void ajouteElt(char * pseudo, char * adip) {
    struct elt *nouveau, *courant, *precedent;

    courant = liste_contacts;
    while(courant != NULL) {
        if(strcmp(courant->adip, adip) == 0 && strcmp(courant->nom, pseudo) == 0) {
            return; 
        }
        courant = courant->next;
    }

    nouveau = malloc(sizeof(struct elt));
    if (!nouveau) return;
    strncpy(nouveau->nom, pseudo, LPSEUDO);
    nouveau->nom[LPSEUDO] = '\0'; 
    strncpy(nouveau->adip, adip, 15);
    nouveau->adip[15] = '\0';
    nouveau->next = NULL;

    courant = liste_contacts;
    precedent = NULL;

    while (courant != NULL && strcmp(courant->nom, pseudo) < 0) {
        precedent = courant;
        courant = courant->next;
    }

    if (precedent == NULL) {
        nouveau->next = liste_contacts;
        liste_contacts = nouveau;
    } else {
        nouveau->next = courant;
        precedent->next = nouveau;
    }
}

void supprimeElt(char * adip) {
    struct elt *courant = liste_contacts;
    struct elt *precedent = NULL;

    while (courant != NULL) {
        if (strcmp(courant->adip, adip) == 0) {
            if (precedent == NULL) {
                liste_contacts = courant->next;
            } else {
                precedent->next = courant->next;
            }
            printf("\n[Info] L'utilisateur %s a quitté le réseau.\nbiceps> ", courant->nom);
            fflush(stdout);
            free(courant); 
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
    if (count == 0) printf(" (Personne pour le moment...)\n");
    printf("---------------------------\n");
}


/* --- FONCTION BROADCAST DYNAMIQUE --- */
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


/* --- COMMANDE UDP --- */
void commande(char octet1, char *message, char *pseudo) {
    if (!serveur_actif) return;

    if (octet1 == '3') {
        pthread_mutex_lock(&mutex_table);
        listeElts(); 
        pthread_mutex_unlock(&mutex_table);

    } else if (octet1 == '4' && pseudo && message) {
        char dest_ip[16] = "";
        int found = 0;

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
        if ((n = recvfrom(sid_global, buf, LBUF - 1, 0, (struct sockaddr *)&client_addr, &ls)) < 0) continue;
        buf[n] = '\0';

        if (strncmp(buf, "WAKEUP", 6) == 0) continue;

        if (n >= 6 && strncmp(&buf[1], "BEUIP", 5) == 0) {
            char code = buf[0];
            char *data = &buf[6];

            if (code == '1' || code == '2') {
                pthread_mutex_lock(&mutex_table);
                ajouteElt(data, inet_ntoa(client_addr.sin_addr));
                pthread_mutex_unlock(&mutex_table);

                if (code == '1') {
                    char ack[LBUF + 16];
                    snprintf(ack, sizeof(ack), "2BEUIP%s", my_pseudo);
                    sendto(sid_global, ack, strlen(ack), 0, (struct sockaddr *)&client_addr, ls);
                }

            } else if (code == '0') {
                pthread_mutex_lock(&mutex_table);
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

/* --- PARTIE 3 : LE SERVEUR TCP ET FICHIERS --- */

// Répond aux requêtes entrantes sur la socket fd
void envoiContenu(int fd) {
    char req_type;
    
    // On lit le 1er octet pour savoir ce qu'on demande ('L' ou 'F')
    if (read(fd, &req_type, 1) <= 0) {
        close(fd);
        return;
    }

    if (req_type == 'L') {
        // Commande LS : on fork et on redirige stdout/stderr vers la socket
        if (fork() == 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            execlp("ls", "ls", "-l", "reppub/", NULL);
            exit(1);
        }
    } else if (req_type == 'F') {
        // Commande GET : on lit le nom du fichier jusqu'au '\n'
        char nomfic[256];
        int i = 0;
        while (read(fd, &nomfic[i], 1) > 0 && nomfic[i] != '\n' && i < 255) {
            i++;
        }
        nomfic[i] = '\0';
        
        char path[512];
        snprintf(path, sizeof(path), "reppub/%s", nomfic);

        if (fork() == 0) {
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            execlp("cat", "cat", path, NULL);
            exit(1);
        }
    }
    // Le parent ferme son fd, l'enfant s'occupe de finir d'écrire dedans
    close(fd);
}

void * serveur_tcp(void * rep) {
    int sid_tcp, s_cli;
    struct sockaddr_in srv_addr, cli_addr;
    socklen_t lcli;
    int opt = 1;

    if ((sid_tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) pthread_exit(NULL);
    if (setsockopt(sid_tcp, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) pthread_exit(NULL);

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(PORT);
    srv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sid_tcp, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) pthread_exit(NULL);
    if (listen(sid_tcp, 5) < 0) pthread_exit(NULL);

    sid_tcp_global = sid_tcp;

    while (serveur_actif) {
        lcli = sizeof(cli_addr);
        s_cli = accept(sid_tcp, (struct sockaddr *)&cli_addr, &lcli);
        if (s_cli < 0) {
            if (!serveur_actif) break;
            continue;
        }
        // Mode factotum simple : on délègue au fork dans envoiContenu
        envoiContenu(s_cli);
    }
    close(sid_tcp);
    sid_tcp_global = -1;
    pthread_exit(NULL);
}

// Commande client pour récupérer la liste
void demandeListe(const char *pseudo) {
    char dest_ip[16] = "";
    int found = 0;

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

    if (!found) {
        printf("Erreur : Pseudo '%s' introuvable.\n", pseudo);
        return;
    }

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return;

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT);
    dest.sin_addr.s_addr = inet_addr(dest_ip);

    if (connect(s, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("connect");
        close(s);
        return;
    }

    write(s, "L", 1);
    char buf[512];
    int n;
    printf("\n--- Fichiers de %s ---\n", pseudo);
    while ((n = read(s, buf, sizeof(buf)-1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }
    printf("------------------------\n");
    close(s);
}

// Commande client pour récupérer un fichier
void demandeFichier(const char *pseudo, const char *nomfic) {
    char dest_ip[16] = "";
    int found = 0;

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

    if (!found) {
        printf("Erreur : Pseudo '%s' introuvable.\n", pseudo);
        return;
    }

    // Sécurité: on vérifie que le fichier n'est pas déjà chez nous
    char local_path[512];
    snprintf(local_path, sizeof(local_path), "reppub/%s", nomfic);
    if (access(local_path, F_OK) == 0) {
        printf("Erreur : Le fichier %s existe déjà en local.\n", nomfic);
        return;
    }

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return;

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT);
    dest.sin_addr.s_addr = inet_addr(dest_ip);

    if (connect(s, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("connect TCP");
        close(s);
        return;
    }

    char req[256];
    snprintf(req, sizeof(req), "F%s\n", nomfic);
    write(s, req, strlen(req));

    int fd = open(local_path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open local");
        close(s);
        return;
    }

    char buf[512];
    int n;
    int total = 0;
    while ((n = read(s, buf, sizeof(buf))) > 0) {
        write(fd, buf, n);
        total += n;
    }
    
    if (total == 0) {
        printf("Erreur : Fichier inexistant ou vide sur le serveur.\n");
        unlink(local_path); // On supprime le fichier vide créé
    } else {
        printf("Fichier %s téléchargé avec succès (%d octets).\n", nomfic, total);
    }

    close(fd);
    close(s);
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
    liste_contacts = NULL;

    // Pour éviter les processus fils zombies générés par les fork() dans envoiContenu
    signal(SIGCHLD, SIG_IGN);

    if (pthread_create(&serveur_thread, NULL, serveur_udp, pseudo_global) != 0) {
        serveur_actif = 0;
        return -1;
    }
    
    if (pthread_create(&serveur_tcp_thread, NULL, serveur_tcp, "reppub") != 0) {
        serveur_actif = 0;
        return -1;
    }

    return 0;
}

int beuip_stop(void) {
    if (!serveur_actif) return -1;

    commande('0', NULL, NULL);
    serveur_actif = 0; 

    // On débloque UDP
    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(PORT);
    local.sin_addr.s_addr = inet_addr("127.0.0.1");
    sendto(sid_global, "WAKEUP", 6, 0, (struct sockaddr *)&local, sizeof(local));

    // On débloque TCP
    if (sid_tcp_global >= 0) {
        shutdown(sid_tcp_global, SHUT_RDWR);
        close(sid_tcp_global);
    }

    pthread_join(serveur_thread, NULL);
    pthread_join(serveur_tcp_thread, NULL);

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

    printf("Serveurs UDP et TCP stoppés. Liste nettoyée.\n");
    return 0;
}

int mess_liste(void) { commande('3', NULL, NULL); return 0; }
int mess_send_one(const char *pseudo, const char *message) { commande('4', (char *)message, (char *)pseudo); return 0; }
int mess_send_all(const char *message) { commande('5', (char *)message, NULL); return 0; }
int beuip_ls(const char *pseudo) { demandeListe(pseudo); return 0; }
int beuip_get(const char *pseudo, const char *nomfic) { demandeFichier(pseudo, nomfic); return 0; }
