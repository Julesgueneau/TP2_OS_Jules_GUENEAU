/* code source de la librairie gescom */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h> /* ajout indispensable pour open et les flags o_rdonly, o_wronly, etc. */
#include "gescom.h"

char *Mots[MAXPAR];
int NMots;

static struct commande_interne tab_com_int[NBMAXC];
static int nb_com_int = 0;

char* copyString(char* s) {
    char* copie;
    if (s == NULL) return NULL;
    copie = malloc(strlen(s) + 1);
    if (copie != NULL) {
        strcpy(copie, s);
    }
    return copie;
}

int analyseCom(char* b) {
    char* token;
    char* delimiteurs = " \t\n";
    NMots = 0;
    while ((token = strsep(&b, delimiteurs)) != NULL) {
        if (*token != '\0') {
            if (NMots < MAXPAR - 1) {
                Mots[NMots] = copyString(token);
                NMots++;
            } else {
                fprintf(stderr, "erreur : nombre maximum de parametres atteint\n");
                break;
            }
        }
    }
    Mots[NMots] = NULL;
    return NMots;
}

void ajouteCom(char *nom, int (*fonc)(int, char **)) {
    if (nb_com_int >= NBMAXC) {
        fprintf(stderr, "erreur fatale : tableau des commandes internes plein\n");
        exit(1);
    }
    tab_com_int[nb_com_int].nom = nom;
    tab_com_int[nb_com_int].fonction = fonc;
    nb_com_int++;
}

int execComInt(int n, char **p) {
    int i;
    if (n == 0) return 0;
    for (i = 0; i < nb_com_int; i++) {
        if (strcmp(p[0], tab_com_int[i].nom) == 0) {
            tab_com_int[i].fonction(n, p);
            return 1;
        }
    }
    return 0;
}

/* fonction statique pour traiter et retirer les redirections du tableau de mots */
static void gererRedirections(void) {
    int i = 0;
    while (Mots[i] != NULL) {
        int decalage = 0;
        
        if (strcmp(Mots[i], "<") == 0 || strcmp(Mots[i], ">") == 0 ||
            strcmp(Mots[i], ">>") == 0 || strcmp(Mots[i], "2>") == 0 ||
            strcmp(Mots[i], "2>>") == 0 || strcmp(Mots[i], "<<") == 0) {

            if (Mots[i+1] == NULL) {
                fprintf(stderr, "erreur de syntaxe : fichier cible manquant\n");
                exit(1);
            }

            if (strcmp(Mots[i], "<") == 0) {
                int fd = open(Mots[i+1], O_RDONLY);
                if (fd < 0) { perror(Mots[i+1]); exit(1); }
                dup2(fd, 0);
                close(fd);
                decalage = 2;
            } else if (strcmp(Mots[i], ">") == 0) {
                /* ecriture avec ecrasement (droits rw-r--r--) */
                int fd = open(Mots[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror(Mots[i+1]); exit(1); }
                dup2(fd, 1);
                close(fd);
                decalage = 2;
            } else if (strcmp(Mots[i], ">>") == 0) {
                /* ecriture avec ajout en fin de fichier */
                int fd = open(Mots[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) { perror(Mots[i+1]); exit(1); }
                dup2(fd, 1);
                close(fd);
                decalage = 2;
            } else if (strcmp(Mots[i], "2>") == 0) {
                /* redirection de la sortie d'erreur standard */
                int fd = open(Mots[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror(Mots[i+1]); exit(1); }
                dup2(fd, 2);
                close(fd);
                decalage = 2;
            } else if (strcmp(Mots[i], "2>>") == 0) {
                int fd = open(Mots[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) { perror(Mots[i+1]); exit(1); }
                dup2(fd, 2);
                close(fd);
                decalage = 2;
            } else if (strcmp(Mots[i], "<<") == 0) {
                /* here-document via un tube anonyme */
                int fdpipe[2];
                if (pipe(fdpipe) == 0) {
                    char buffer[1024];
                    while (1) {
                        write(1, "> ", 2); /* prompt simule pour le heredoc */
                        int n = read(0, buffer, sizeof(buffer)-1);
                        if (n <= 0) break;
                        buffer[n] = '\0';
                        /* arret si on rencontre exactement le delimiteur suivi d'un saut de ligne */
                        if (strncmp(buffer, Mots[i+1], strlen(Mots[i+1])) == 0 &&
                            buffer[strlen(Mots[i+1])] == '\n') {
                            break;
                        }
                        write(fdpipe[1], buffer, n);
                    }
                    close(fdpipe[1]);
                    dup2(fdpipe[0], 0); /* l'entree du processus devient la sortie du tube */
                    close(fdpipe[0]);
                }
                decalage = 2;
            }

            /* nettoyage de la memoire et decalage des arguments restants vers la gauche */
            free(Mots[i]);
            free(Mots[i+1]);
            int j = i;
            while (Mots[j + decalage] != NULL) {
                Mots[j] = Mots[j + decalage];
                j++;
            }
            Mots[j] = NULL;
            NMots -= decalage;
        } else {
            i++;
        }
    }
}

int execComExt(char **p) {
    pid_t pid;
    int status;
    pid = fork();
    if (pid < 0) {
        perror("erreur fork");
        return -1;
    }
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        /* application des redirections dans le contexte du fils */
        gererRedirections();
        execvp(Mots[0], Mots);
        perror(Mots[0]);
        exit(1);
    } else {
        waitpid(pid, &status, 0);
    }
    return 0;
}

void execPipeline(char **cmds, int nb) {
    int i;
    int fd[2];
    int fd_in = 0;
    pid_t pids[MAXPAR];

    for (i = 0; i < nb; i++) {
        pipe(fd);
        pids[i] = fork();
        
        if (pids[i] < 0) {
            perror("erreur fork pipe");
            return;
        }
        
        if (pids[i] == 0) {
            signal(SIGINT, SIG_DFL);
            dup2(fd_in, 0);
            if (i < nb - 1) {
                dup2(fd[1], 1);
            }
            close(fd[0]);
            
            analyseCom(cmds[i]);
            /* les redirections explicites ecrasent celles du pipe si necessaire */
            gererRedirections();
            execvp(Mots[0], Mots);
            perror(Mots[0]);
            exit(1);
        } else {
            close(fd[1]);
            if (fd_in != 0) close(fd_in);
            fd_in = fd[0];
        }
    }
    
    for (i = 0; i < nb; i++) {
        waitpid(pids[i], NULL, 0);
    }
}