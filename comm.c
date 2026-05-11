#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include "protocole.h"

//recuperer ip pour signer les messages
void obtenir_ip_locale(char *ip) {
    char host[256];
    gethostname(host, 256);
    struct hostent *hp = gethostbyname(host);
    if (hp) {
        strcpy(ip, inet_ntoa(*(struct in_addr *)hp->h_addr));
    } else {
        strcpy(ip, "127.0.0.1");
    }
}

//lecture du buffer
ssize_t read_all(int fd, void *buf, size_t length) {
    size_t total = 0;
    while (total < length) {
        ssize_t n = read(fd, (char *)buf + total, length - total);
        if (n <= 0) return n;
        total += n;
    }
    return (ssize_t)total;
}

//connexion au driver
int se_connecter(char *host, int port) {
    struct hostent *hp = gethostbyname(host);
    struct sockaddr_in serv;
    if (!hp) return -1;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    memcpy(&serv.sin_addr, hp->h_addr, hp->h_length);
    if (connect(s, (struct sockaddr *)&serv, sizeof(serv)) == -1) return -1;
    return s;
}
//menu
void menu() {
    printf("\n--- INTERFACE ANNEAU ---\n");
    printf("1. Message Direct\n2. Diffusion (Tous)\n3. Etat de l'anneau (Graphique)\n");
    printf("4. Envoyer Fichier\n5. Regenerer Jeton (SOS)\n6. Quitter\n");
    printf("Choix : ");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc != 3) { 
        printf("Usage: %s <ip_driver> <port_local_driver>\n", argv[0]); 
        exit(1); 
    }
    
    char ip_loc[50];
    obtenir_ip_locale(ip_loc);
    int sock = se_connecter(argv[1], atoi(argv[2]));
    if (sock == -1) { 
        perror("Connexion Driver echouee"); 
        exit(1); 
    }

    fd_set readfds;
    Paquet p;
    menu();

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);
        //attend
        select(sock + 1, &readfds, NULL, NULL, NULL);
        //on recupere depuis le driver
        if (FD_ISSET(sock, &readfds)) {
            if (read_all(sock, &p, sizeof(Paquet)) <= 0) { 
                printf("\n[ERREUR] Driver deconnecte.\n"); 
                exit(0); 
            }
            //on recupere le fichier en plusieurs fois
            if (p.type == TYPE_FICHIER) {
                char nom_f[128]; 
                snprintf(nom_f, 128, "recu_%s", p.id_dest); 
                if (p.taille == -1) {
                    //on cree le fichier vide
                    FILE *f = fopen(nom_f, "wb");
                    if (f) fclose(f);
                    printf("\n[FICHIER] Debut de la reception : %s...\n", nom_f);
                } 
                else if (p.taille > 0) {
                    //on ajoute a la fin du fichier
                    FILE *f = fopen(nom_f, "ab");
                    if (f) { 
                        fwrite(p.buffer, 1, p.taille, f); 
                        fclose(f); 
                    }
                } 
                else if (p.taille == 0) {
                    //signal de fin
                    printf("\n[FICHIER] Reception terminee avec succes : %s\n", nom_f);
                }
            } 
            else if (p.type == TYPE_INFO) {
                //affichage de la topologie
                printf("\n==========================================\n");
                printf("          TOPOLOGIE DE L'ANNEAU           \n");
                printf("==========================================\n\n"); 
                char copy[TAILLE_MAX];
                strcpy(copy, p.buffer);
                char *passage = strtok(copy, ";");   
                while(passage != NULL) {
                    char nom[100], ip[50];
                    int pe, ps;
                    if (sscanf(passage, "%[^|]|%[^|]|%d|%d", nom, ip, &pe, &ps) == 4) {
                        printf("        +-----------------------------+\n");
                        printf("        | Nom    : %-18s |\n", nom);
                        printf("        | IP     : %-18s |\n", ip);
                        printf("        | Port E : %-18d |\n", pe);
                        printf("        | Port S : %-18d |\n", ps);
                        printf("        +-----------------------------+\n");
                        printf("                       |\n");
                        printf("                       v\n");
                    }
                    passage = strtok(NULL, ";");
                }
                printf("                (Retour a %s)\n", p.id_src);
                printf("\n==========================================\n");
            } else {
                printf("\n[MSG] %s : %s\n", p.id_src, p.buffer);
            }
            menu();
        }

        //menu de l'utilisateur
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            int choix;
            if (scanf("%d", &choix) <= 0) break;
            while(getchar() != '\n'); 
            memset(&p, 0, sizeof(p));
            if (choix == 1 || choix == 2) {
                p.type = (choix == 1) ? TYPE_MESSAGE : TYPE_DIFFUSION;
                strcpy(p.id_src, ip_loc);      
                if (choix == 1) { 
                    printf("IP Destination : "); 
                    scanf("%s", p.id_dest); 
                    while(getchar() != '\n'); 
                } else {
                    strcpy(p.id_dest, "TOUS");
                }     
                printf("Message : "); 
                fgets(p.buffer, TAILLE_MAX, stdin);
                p.buffer[strcspn(p.buffer, "\n")] = '\0';
            } 
            else if (choix == 4) {
                //envoi de fichier
                char chemin[100]; 
                printf("Chemin du fichier : "); scanf("%s", chemin);
                printf("IP Cible : "); scanf("%s", p.id_src);      
                FILE *f = fopen(chemin, "rb");
                if (!f) { 
                    printf("Erreur : Impossible d'ouvrir le fichier.\n"); 
                    menu();
                    continue; 
                }      
                char *nom = strrchr(chemin, '/');
                strcpy(p.id_dest, nom ? nom+1 : chemin);
                printf("[FICHIER] Preparation de l'envoi...\n");
                //envoie signal de depart
                p.type = TYPE_FICHIER;
                p.taille = -1;
                write(sock, &p, sizeof(p));
                usleep(30000); //pause pour pas que le jeton arrive avant le signal de depart

                //boucle pour l envoi du fichier en plusieurs paquets
                while ((p.taille = fread(p.buffer, 1, TAILLE_MAX, f)) > 0) {
                    p.type = TYPE_FICHIER;
                    write(sock, &p, sizeof(p));
                    usleep(30000); //evite de faire saturer le driver
                }
                fclose(f);
                //envoi du signal de fin
                p.type = TYPE_FICHIER;
                p.taille = 0;
                //le paquet sera envoye par bloc
                printf("[FICHIER] Envoi termine.\n");
            } 
            else if (choix == 5) { 
                p.type = TYPE_JETON; 
            }
            else if (choix == 6) { 
                p.type = TYPE_BYE; 
                write(sock, &p, sizeof(p)); 
                printf("Fermeture de l'interface...\n");
                exit(0); 
            }
            else if (choix == 3) { 
                p.type = TYPE_INFO; 
                strcpy(p.id_src, ip_loc);
                strcpy(p.buffer, ""); 
            }
            
            //envoi ver le driver
            if (choix >= 1 && choix <= 5) {
                write(sock, &p, sizeof(p));
            }
            menu();
        }
    }
    return 0;
}