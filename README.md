# TP3 OS User - BICEPS v3 (Bel Interpréteur de Commandes des Elèves de Polytech Sorbonne)

**Auteur :** Roman Boyer  
**École :** Polytech Sorbonne (Filière Électronique et Informatique)  
**Module :** Système d'Exploitation (OS User)

## Description
Ce projet est l'aboutissement du TP3 d'OS User. Il s'agit de l'évolution du programme **biceps**, un interpréteur de commandes réseau permettant à des utilisateurs d'échanger des messages et des fichiers sur un réseau local via les protocoles UDP et TCP.

Cette version 3 intègre le multi-threading, la gestion dynamique des interfaces réseau et un serveur de partage de fichiers.

## Fonctionnalités implémentées
* **Architecture Multi-thread :** L'interpréteur de commandes, le serveur UDP (messages) et le serveur TCP (fichiers) cohabitent dans le même processus, avec une gestion des accès concurrents via des mutex.
* **Sécurité accrue :** Remplacement des messages en boucle locale par des appels de fonctions internes pour contrer les attaques de type "man-in-the-middle".
* **Réseau dynamique :** Détection automatique des interfaces réseau actives (`getifaddrs`) et envoi de broadcast dynamique.
* **Gestion des contacts :** Utilisation d'une liste chaînée dynamique pour stocker les utilisateurs connectés.
* **Partage de fichiers (TCP) :** Possibilité d'explorer le répertoire public d'un utilisateur distant et de télécharger ses fichiers.

## Fichiers principaux
* `triceps.c` : Code source de l'interpréteur de commandes interactif.
* `creme.c` / `creme.h` : Librairie réseau (Commandes Rapides pour l'Envoi de Messages Evolués) contenant la logique client/serveur UDP et TCP.
* `Makefile` : Fichier de compilation automatisée.
* `reppub/` : Répertoire local public utilisé pour stocker les fichiers à partager.

## Compilation
Un `Makefile` est fourni pour compiler le projet avec les options strictes (`-Wall -Werror`) et le support des threads (`-pthread`).

Pour tout compiler :
```bash
make
