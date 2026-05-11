💍 Simulation de Communication en Anneau (Token Ring)
Ce projet consiste en la création d'un réseau local structuré en anneau, inspiré du protocole IEEE 802.5 (Token Ring). Il a été développé en langage C dans le cadre du module de Protocoles Réseaux.

📌 Présentation du Projet
L'objectif est de permettre à plusieurs machines de communiquer de manière ordonnée et sans collisions. Le principe repose sur un jeton unique qui circule de nœud en nœud : seule la machine possédant le jeton est autorisée à émettre des données.

Caractéristiques principales :
Dynamisme : Possibilité d'insérer une nouvelle machine ou d'en retirer une sans briser l'anneau.

Fiabilité : Support de l'envoi de messages simples et du transfert de fichiers.

Architecture bi-processus : Chaque nœud du réseau est composé de deux entités distinctes.

🏗️ Architecture Technique
Le système sépare la logique réseau de l'interface utilisateur grâce à deux processus :

Le Driver (driver.c) :

Agit comme un routeur en tâche de fond.

Écoute le voisin de gauche (via socket TCP) et transmet au voisin de droite.

Gère la circulation du jeton et la reconfiguration de l'anneau lors de l'arrivée/départ d'un nœud.

Le Comm (comm.c) :

Interface utilisateur (IHM) en ligne de commande.

Permet à l'utilisateur de saisir des messages ou de choisir des fichiers à envoyer.

Communique avec son Driver local via des sockets.

📦 Protocoles et Paquets
Les échanges sont structurés via une structure Paquet définie dans protocole.h :

Types de paquets : JETON, MESSAGE, DIFFUSION (Broadcast), FICHIER, HELLO/BYE (pour la dynamique de l'anneau).

Adressage : Identification des nœuds par leur adresse IP.

🛠️ Compilation et Utilisation
1. Compilation
Utilisez un compilateur C (gcc) pour générer les exécutables :

gcc -o driver driver.c
gcc -o comm comm.c
2. Lancement du réseau
Premier nœud (création de l'anneau) :

./driver [port_écoute]
Nœuds suivants (insertion dans l'anneau) :

./driver [port_écoute] [ip_voisin] [port_voisin]
Lancer l'interface de communication :
Dans un autre terminal sur la même machine :

./comm [ip_driver_local] [port_driver_local]

👥 Auteurs
Projet réalisé par Dimitri Botella et Nathan Bartier (Licence 3 Informatique).





































































