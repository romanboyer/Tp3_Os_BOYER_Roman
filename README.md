# TP3 OS User - BICEPS v3 (Bel Interpréteur de Commandes des Elèves de Polytech Sorbonne)

**Auteur :** Roman Boyer  
**École :** Polytech Sorbonne (Filière Électronique et Informatique)  
**Module :** Système d'Exploitation (OS User)

## Description
Ce projet est l'aboutissement du TP3 d'OS User. Il s'agit de l'évolution du programme **biceps**, un interpréteur de commandes réseau permettant à des utilisateurs d'échanger des messages et des fichiers sur un réseau local via les protocoles UDP et TCP.

Cette version intègre le multi-threading, le partage de données sécurisées entre threads (mutex) et la gestion des utilisateurs sous forme de liste chaînée dynamique (partie 2.2). Une fonctionnalité bonus de partage de fichiers via TCP a également été intégrée (Partie 3).

## Commandes supportées
* `beuip start <pseudo>` : Lance les serveurs d'écoute.
* `beuip list` : Affiche la liste des utilisateurs connectés (Format `IP : Pseudo`).
* `beuip message <pseudo> <message>` : Envoie un message privé.
* `beuip message all <message>` : Diffuse un message à tous les utilisateurs.
* `beuip stop` : Quitte le réseau et stoppe les serveurs.
* `exit` (ou `CTRL+D`) : Stoppe les serveurs proprement et ferme le programme.

*Bonus TCP :*
* `beuip ls <pseudo>` : Liste les fichiers du répertoire public distant.
* `beuip get <pseudo> <fichier>` : Télécharge un fichier distant.

## Compilation et Tests
Le Makefile permet de recompiler l'ensemble du projet sans générer de fichiers résiduels en conservant les flags `-Wall -Werror`.

**Compiler l'exécutable classique :**
```bash
make
./biceps
