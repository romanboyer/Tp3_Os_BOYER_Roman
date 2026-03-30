#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9998
#define MAX_CLIENTS 255
#define LBUF 512
#define BCAST_IP "192.168.88.255"

struct Couple {
    struct in_addr ip;
    char pseudo[50];
};

struct Couple table[MAX_CLIENTS];
int nb_couples = 0;

/* Variables globales pour le signal */
int sid_global;
char pseudo_global[50];

void gerer_signal(int sig) {
    char buf[LBUF];
    struct sockaddr_in bcast_addr;
    
    memset(&bcast_addr, 0, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(PORT);
    bcast_addr.sin_addr.s_addr = inet_addr(BCAST_IP);

    snprintf(buf, LBUF, "0BEUIP%s", pseudo_global);
    sendto(sid_global, buf, strlen(buf), 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));
    
    printf("\nSignal %d reçu : arrêt du serveur et envoi du broadcast de déconnexion.\n", sig);
    exit(0);
}

int main(int argc, char *argv[]) {
    int sid, n;
    struct sockaddr_in SockConf, client_addr, bcast_addr;
    socklen_t ls;
    char buf[LBUF];
    int opt = 1;

    if (argc != 2) {
        fprintf(stderr, "Utilisation: %s <pseudo>\n", argv[0]);
        return 1;
    }
    
    /* Copie du pseudo en variable globale pour le gestionnaire de signaux */
    strncpy(pseudo_global, argv[1], 49);
    pseudo_global[49] = '\0';
    char *my_pseudo = pseudo_global;

    if ((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 2;
    }
    
    /* Affectation du socket global pour le gestionnaire de signaux */
    sid_global = sid;

    /* Mise en place du gestionnaire de signaux */
    signal(SIGTERM, gerer_signal);
    signal(SIGINT, gerer_signal);

    if (setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        return 3;
    }

    memset(&SockConf, 0, sizeof(SockConf));
    SockConf.sin_family = AF_INET;
    SockConf.sin_port = htons(PORT);
    SockConf.sin_addr.s_addr = INADDR_ANY;

    if (bind(sid, (struct sockaddr *)&SockConf, sizeof(SockConf)) < 0) {
        perror("bind");
        return 4;
    }

    printf("Serveur BEUIP (%s) sur port %d.\n", my_pseudo, PORT);

    /* Envoi initial broadcast (Code 1) */
    memset(&bcast_addr, 0, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(PORT);
    bcast_addr.sin_addr.s_addr = inet_addr(BCAST_IP);

    char init_msg[LBUF + 16];
    snprintf(init_msg, sizeof(init_msg), "1BEUIP%s", my_pseudo);
    sendto(sid, init_msg, strlen(init_msg), 0, (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));

    for (;;) {
        ls = sizeof(client_addr);
        memset(buf, 0, LBUF);
        if ((n = recvfrom(sid, buf, LBUF - 1, 0, (struct sockaddr *)&client_addr, &ls)) < 0) {
            continue;
        }
        buf[n] = '\0';

        if (n >= 6 && strncmp(&buf[1], "BEUIP", 5) == 0) {
            char code = buf[0];
            char *data = &buf[6];

            /* Sécurité : on rejette les codes locaux s'ils ne viennent pas de 127.0.0.1 */
            if ((code == '3' || code == '4' || code == '5') && 
                client_addr.sin_addr.s_addr != inet_addr("127.0.0.1")) {
                fprintf(stderr, "Alerte : tentative d'intrusion rejetée (code %c depuis %s)\n", 
                        code, inet_ntoa(client_addr.sin_addr));
                continue;
            }

            if (code == '1' || code == '2') {
                /* Ajout dans la table */
                int found = 0;
                for (int i = 0; i < nb_couples; i++) {
                    if (table[i].ip.s_addr == client_addr.sin_addr.s_addr && strcmp(table[i].pseudo, data) == 0) {
                        found = 1; break;
                    }
                }
                if (!found && nb_couples < MAX_CLIENTS) {
                    table[nb_couples].ip = client_addr.sin_addr;
                    strncpy(table[nb_couples].pseudo, data, 49);
                    nb_couples++;
                }
                /* Envoi AR si code 1 */
                if (code == '1') {
                    char ack[LBUF + 16];
                    snprintf(ack, sizeof(ack), "2BEUIP%s", my_pseudo);
                    sendto(sid, ack, strlen(ack), 0, (struct sockaddr *)&client_addr, ls);
                }

            } else if (code == '0') {
                /* Suppression du couple quittant le réseau */
                for (int i = 0; i < nb_couples; i++) {
                    if (table[i].ip.s_addr == client_addr.sin_addr.s_addr && strcmp(table[i].pseudo, data) == 0) {
                        table[i] = table[nb_couples - 1]; /* Remplace par le dernier élément */
                        nb_couples--;
                        printf("L'utilisateur %s a quitté le réseau.\n", data);
                        break;
                    }
                }

            } else if (code == '3') {
                /* Affichage de la table */
                printf("\n--- Liste des connectés (%d) ---\n", nb_couples);
                for (int i = 0; i < nb_couples; i++) {
                    printf(" - %s (%s)\n", table[i].pseudo, inet_ntoa(table[i].ip));
                }
                printf("-------------------------------\n");

            } else if (code == '4') {
                /* Envoi message (Code 9) à un pseudo précis */
                char *target_pseudo = data;
                char *msg_content = data + strlen(target_pseudo) + 1;
                int found = 0;

                for (int i = 0; i < nb_couples; i++) {
                    if (strcmp(table[i].pseudo, target_pseudo) == 0) {
                        struct sockaddr_in dest;
                        memset(&dest, 0, sizeof(dest));
                        dest.sin_family = AF_INET;
                        dest.sin_port = htons(PORT);
                        dest.sin_addr = table[i].ip;

                        char msg9[LBUF + 16];
                        snprintf(msg9, sizeof(msg9), "9BEUIP%s", msg_content);
                        sendto(sid, msg9, strlen(msg9), 0, (struct sockaddr *)&dest, sizeof(dest));
                        found = 1;
                        break;
                    }
                }
                if (!found) printf("Erreur : Pseudo '%s' introuvable.\n", target_pseudo);

            } else if (code == '5') {
                /* Envoi message (Code 9) à tout le monde */
                char msg9[LBUF + 16];
                snprintf(msg9, sizeof(msg9), "9BEUIP%s", data);
                
                for (int i = 0; i < nb_couples; i++) {
                    struct sockaddr_in dest;
                    memset(&dest, 0, sizeof(dest));
                    dest.sin_family = AF_INET;
                    dest.sin_port = htons(PORT);
                    dest.sin_addr = table[i].ip;
                    sendto(sid, msg9, strlen(msg9), 0, (struct sockaddr *)&dest, sizeof(dest));
                }

            } else if (code == '9') {
                /* Réception d'un message : recherche de l'expéditeur via son IP */
                int found = 0;
                for (int i = 0; i < nb_couples; i++) {
                    if (table[i].ip.s_addr == client_addr.sin_addr.s_addr) {
                        printf("Message de %s : %s\n", table[i].pseudo, data);
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    printf("Erreur : Message d'un expéditeur inconnu (%s) : %s\n", 
                           inet_ntoa(client_addr.sin_addr), data);
                }
            }
        }
    }
    return 0;
}