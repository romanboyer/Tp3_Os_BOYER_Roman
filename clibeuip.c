#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9998
#define LBUF 512

int main(int argc, char *argv[]) {
    int sid;
    struct sockaddr_in Sock;
    char buf[LBUF];
    int len;

    if (argc < 2) {
        fprintf(stderr, "Utilisation:\n");
        fprintf(stderr, "  %s liste\n", argv[0]);
        fprintf(stderr, "  %s msg <pseudo> <message>\n", argv[0]);
        fprintf(stderr, "  %s all <message>\n", argv[0]);
        return 1;
    }

    if ((sid = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        return 2;
    }

    memset(&Sock, 0, sizeof(Sock));
    Sock.sin_family = AF_INET;
    Sock.sin_port = htons(PORT);
    Sock.sin_addr.s_addr = inet_addr("127.0.0.1");

    memset(buf, 0, LBUF);

    if (strcmp(argv[1], "liste") == 0) {
        /* Commande 3 : Liste */
        snprintf(buf, LBUF, "3BEUIP");
        sendto(sid, buf, strlen("3BEUIP"), 0, (struct sockaddr *)&Sock, sizeof(Sock));
        printf("Commande 'liste' envoyée au serveur local.\n");

    } else if (strcmp(argv[1], "msg") == 0 && argc >= 4) {
        /* Commande 4 : Message à un pseudo précis (séparateur \0) */
        buf[0] = '4';
        strcpy(&buf[1], "BEUIP");
        int len_pseudo = strlen(argv[2]);
        strcpy(&buf[6], argv[2]);
        strcpy(&buf[6 + len_pseudo + 1], argv[3]); /* Le +1 saute le \0 inséré par le strcpy précédent */
        
        len = 6 + len_pseudo + 1 + strlen(argv[3]);
        sendto(sid, buf, len, 0, (struct sockaddr *)&Sock, sizeof(Sock));
        printf("Commande 'msg' envoyée pour %s.\n", argv[2]);

    } else if (strcmp(argv[1], "all") == 0 && argc >= 3) {
        /* Commande 5 : Message à tout le monde */
        snprintf(buf, LBUF, "5BEUIP%s", argv[2]);
        sendto(sid, buf, strlen(buf), 0, (struct sockaddr *)&Sock, sizeof(Sock));
        printf("Commande 'all' envoyée.\n");

    } else {
        fprintf(stderr, "Erreur : Commande inconnue ou paramètres manquants.\n");
    }

    return 0;
}