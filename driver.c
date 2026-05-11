#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <signal.h>
#include "protocole.h"

//variables
char ip_d[50]; int port_d;
char ip_loc[50]; int port_g_loc;
int sockg = -1, sockd = -1, sockl = -1;
int a_emettre = 0;
Paquet p_emettre;

void obtenir_ip_locale(char *ip) {
    char host[256]; gethostname(host, 256);
    struct hostent *hp = gethostbyname(host);
    if (hp) strcpy(ip, inet_ntoa(*(struct in_addr *)hp->h_addr));
}

ssize_t read_ex(int fd, void *buf, size_t len) {
    size_t t = 0;
    while (t < len) {
        ssize_t c = read(fd, (char *)buf + t, len - t);
        if (c <= 0) return c;
        t += c;
    }
    return t;
}
//on se connecte a droite
void connecter_droit() {
    if (sockd != -1) close(sockd);
    sockd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in adr;
    adr.sin_family = AF_INET; adr.sin_port = htons(port_d);
    inet_pton(AF_INET, ip_d, &adr.sin_addr);
    
    if (connect(sockd, (struct sockaddr *)&adr, sizeof(adr)) == 0) {
        printf("[OK] Connecte au voisin droit %s:%d\n", ip_d, port_d);
        Paquet h = {TYPE_HELLO, 0};
        snprintf(h.buffer, TAILLE_MAX, "%d", port_g_loc);
        write(sockd, &h, sizeof(h));
    } else {
        close(sockd); sockd = -1;
        sleep(1); 
    }
}

int main(int argc, char *argv[]) {
    if (argc < 5) { printf("Usage: %s <port_g> <port_l> <ip_d> <port_d>\n", argv[0]); exit(1); }
    
    signal(SIGPIPE, SIG_IGN);
    port_g_loc = atoi(argv[1]);
    int s_ec_g = socket(AF_INET, SOCK_STREAM, 0);
    int s_ec_l = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(s_ec_g, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(s_ec_l, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in serv_g, serv_l;
    serv_g.sin_family = AF_INET; serv_g.sin_addr.s_addr = INADDR_ANY; serv_g.sin_port = htons(port_g_loc);
    serv_l.sin_family = AF_INET; serv_l.sin_addr.s_addr = INADDR_ANY; serv_l.sin_port = htons(atoi(argv[2]));
    bind(s_ec_g, (struct sockaddr *)&serv_g, sizeof(serv_g)); listen(s_ec_g, 5);
    bind(s_ec_l, (struct sockaddr *)&serv_l, sizeof(serv_l)); listen(s_ec_l, 5);
    strcpy(ip_d, argv[3]); port_d = atoi(argv[4]);
    obtenir_ip_locale(ip_loc);
    printf("DRIVER [%s] en attente sur port G:%d...\n", ip_loc, port_g_loc);

    while (1) {
        fd_set fds; FD_ZERO(&fds);
        FD_SET(s_ec_g, &fds); FD_SET(s_ec_l, &fds);
        int max = (s_ec_g > s_ec_l) ? s_ec_g : s_ec_l;
        if (sockg != -1 && sockd != -1) { FD_SET(sockg, &fds); if(sockg > max) max = sockg; }
        if (sockd != -1) { FD_SET(sockd, &fds); if(sockd > max) max = sockd; }
        if (sockl != -1) { FD_SET(sockl, &fds); if(sockl > max) max = sockl; }
        struct timeval tv = {1, 0};
        if (sockd == -1) connecter_droit();
        int res = select(max + 1, &fds, NULL, NULL, &tv);
        if (res < 0) continue;
        //interface de l anneau
        if (FD_ISSET(s_ec_l, &fds)) {
            if (sockl != -1) close(sockl);
            sockl = accept(s_ec_l, NULL, NULL);
        }
        if (sockl != -1 && FD_ISSET(sockl, &fds)) {
            if (read_ex(sockl, &p_emettre, sizeof(Paquet)) <= 0) { close(sockl); sockl = -1; }
            else {
                if (p_emettre.type == TYPE_BYE) {
                    printf("[DEPART] Fermeture propre de l'anneau...\n");
                    Paquet b = {TYPE_BYE, 0}; 
                    strcpy(b.id_dest, ip_loc); 
                    snprintf(b.buffer, TAILLE_MAX, "%s:%d", ip_d, port_d);
                    if (sockd != -1) write(sockd, &b, sizeof(b));
                    sleep(1); 
                    exit(0);
                }
                if (p_emettre.type == TYPE_JETON && sockd != -1) write(sockd, &p_emettre, sizeof(Paquet));
                else { if (p_emettre.type != TYPE_FICHIER) strcpy(p_emettre.id_src, ip_loc); a_emettre = 1; }
            }
        }

        //insere un voisin gauche
        if (FD_ISSET(s_ec_g, &fds)) {
            struct sockaddr_in client_adr; socklen_t len = sizeof(client_adr);
            int new_g = accept(s_ec_g, (struct sockaddr *)&client_adr, &len);
            char ip_new[50]; strcpy(ip_new, inet_ntoa(client_adr.sin_addr));
            
            Paquet h;
            if (read_ex(new_g, &h, sizeof(h)) > 0 && h.type == TYPE_HELLO) {
                if (sockg != -1) {
                    Paquet rc = {TYPE_RECONFIG, 0};
                    snprintf(rc.buffer, TAILLE_MAX, "%s:%s", ip_new, h.buffer);
                    write(sockg, &rc, sizeof(rc));
                    close(sockg);
                }
                sockg = new_g;
                printf("[ANNEAU] Nouveau voisin gauche : %s (Port d'ecoute : %s)\n", ip_new, h.buffer);
            }
        }

        //reconfig du voisin droit
        if (sockd != -1 && FD_ISSET(sockd, &fds)) {
            Paquet p;
            if (read_ex(sockd, &p, sizeof(p)) <= 0) {
                close(sockd); sockd = -1;
            } else if (p.type == TYPE_RECONFIG) {
                printf("[RECONFIG] Ma nouvelle cible droite : %s\n", p.buffer);
                char *nip = strtok(p.buffer, ":"); char *npt = strtok(NULL, ":");
                if (nip && npt) { strcpy(ip_d, nip); port_d = atoi(npt); close(sockd); sockd = -1; }
            }
        }

        //trafic depuis la gauche
        if (sockg != -1 && sockd != -1 && FD_ISSET(sockg, &fds)) {
            Paquet p;
            if (read_ex(sockg, &p, sizeof(p)) <= 0) { 
                printf("[ALERTE] Voisin gauche deconnecte brutalement.\n");
                close(sockg); sockg = -1; 
            }
            else {
                if (p.type == TYPE_JETON) {
                    if (a_emettre && sockd != -1) { 
                        //ajout des infos pour la topologie
                        if (p_emettre.type == TYPE_INFO) {
                            char mon_nom[256]; gethostname(mon_nom, 256);
                            char *dot = strchr(mon_nom, '.'); if (dot) *dot = '\0';
                            
                            struct sockaddr_in sin_g; socklen_t len_g = sizeof(sin_g);
                            int p_ent = port_g_loc;
                            if (getsockname(s_ec_g, (struct sockaddr *)&sin_g, &len_g) == 0) p_ent = ntohs(sin_g.sin_port);
                            
                            struct sockaddr_in sin_d; socklen_t len_d = sizeof(sin_d);
                            int p_sor = port_d;
                            if (getpeername(sockd, (struct sockaddr *)&sin_d, &len_d) == 0) p_sor = ntohs(sin_d.sin_port);
                            
                            snprintf(p_emettre.buffer, TAILLE_MAX, "%s|%s|%d|%d;", mon_nom, ip_loc, p_ent, p_sor);
                        }
                        write(sockd, &p_emettre, sizeof(p_emettre)); 
                        a_emettre = 0; 
                    }
                    if (sockd != -1) write(sockd, &p, sizeof(p));
                }
                else if (p.type == TYPE_BYE) {
                    if (strcmp(p.id_dest, ip_d) == 0) {
                        printf("[DECONNEXION] Mon voisin droit s'en va. Reconnexion vers %s\n", p.buffer);
                        char *nip = strtok(p.buffer, ":"); char *npt = strtok(NULL, ":");
                        if (nip && npt) { strcpy(ip_d, nip); port_d = atoi(npt); close(sockd); sockd = -1; }
                    } else {
                        if (sockd != -1) write(sockd, &p, sizeof(p));
                    }
                }
                //les machines font circuler les messages et les fichiers
                else if (p.type == TYPE_INFO) {
                    if (strcmp(p.id_src, ip_loc) == 0) { 
                        if (sockl != -1) write(sockl, &p, sizeof(p)); 
                    }
                    else { 
                        char ma_trace[256];
                        char mon_nom[256]; gethostname(mon_nom, 256);
                        char *dot = strchr(mon_nom, '.'); if (dot) *dot = '\0';              
                        //recuperer les ports d'ecoute et de sortie pour la topologie
                        struct sockaddr_in sin_g; socklen_t len_g = sizeof(sin_g);
                        int p_ent = port_g_loc;
                        if (getsockname(s_ec_g, (struct sockaddr *)&sin_g, &len_g) == 0) p_ent = ntohs(sin_g.sin_port);              
                        struct sockaddr_in sin_d; socklen_t len_d = sizeof(sin_d);
                        int p_sor = port_d;
                        if (getpeername(sockd, (struct sockaddr *)&sin_d, &len_d) == 0) p_sor = ntohs(sin_d.sin_port);               
                        snprintf(ma_trace, 256, "%s|%s|%d|%d;", mon_nom, ip_loc, p_ent, p_sor);
                        strncat(p.buffer, ma_trace, TAILLE_MAX - strlen(p.buffer) - 1);               
                        if (sockd != -1) write(sockd, &p, sizeof(p)); 
                    }
                }
                else { //messages et fichiers
                    int pour_moi = (p.type == TYPE_FICHIER) ? (strcmp(p.id_src, ip_loc) == 0) : (strcmp(p.id_dest, ip_loc) == 0 || strcmp(p.id_dest, "TOUS") == 0);
                    if (pour_moi && sockl != -1) write(sockl, &p, sizeof(p));
                    int emetteur = (p.type != TYPE_FICHIER && strcmp(p.id_src, ip_loc) == 0);
                    if (((p.type == TYPE_DIFFUSION && !emetteur) || (!pour_moi && !emetteur)) && sockd != -1)
                        write(sockd, &p, sizeof(p));
                }
            }
        }
    }
    return 0;
}