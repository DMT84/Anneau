#ifndef PROTOCOLE_H
#define PROTOCOLE_H

#define TAILLE_MAX 1024

//differents types de paquets
#define TYPE_JETON     0
#define TYPE_MESSAGE   1
#define TYPE_DIFFUSION 2
#define TYPE_FICHIER   3
#define TYPE_INFO      4
#define TYPE_BYE       5
#define TYPE_HELLO     6
#define TYPE_RECONFIG  7

typedef struct {
    int type;
    int taille;
    char id_dest[50];
    char id_src[50];
    char buffer[TAILLE_MAX]; 
} Paquet;

#endif