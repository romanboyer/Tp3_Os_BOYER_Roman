#ifndef CREME_H
#define CREME_H

/* Démarre le serveur en tâche de fond */
int beuip_start(const char *pseudo);

/* Arrête le serveur proprement */
int beuip_stop(void);

/* Envoie la commande liste au serveur local */
int mess_liste(void);

/* Envoie un message à un pseudo précis */
int mess_send_one(const char *pseudo, const char *message);

/* Envoie un message à tous les connectés */
int mess_send_all(const char *message);

/* Demande la liste des fichiers à un pseudo */
int beuip_ls(const char *pseudo);

/* Télécharge un fichier depuis un pseudo */
int beuip_get(const char *pseudo, const char *nomfic);

#endif
