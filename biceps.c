#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "gescom.h"
#include "creme.h"

#define HIST_FILE ".biceps_history"

/* variable globale pour le serveur */
pid_t pid_beuip = -1;

/* generation du texte du prompt */
char* creer_prompt(void) {
    char* user;
    char hostname[256];
    char suffixe;
    int taille;
    char* prompt;

    user = getenv("USER");
    if (user == NULL) user = "inconnu";
    
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "machine");
    }
    
    if (geteuid() == 0) suffixe = '#';
    else suffixe = '$';
    
    taille = snprintf(NULL, 0, "%s@%s%c ", user, hostname, suffixe);
    prompt = malloc(taille + 1);
    if (prompt != NULL) {
        snprintf(prompt, taille + 1, "%s@%s%c ", user, hostname, suffixe);
    }
    return prompt;
}

/* gestion de la sortie du programme */
int Sortie(int n, char *p[]) {
    write_history(HIST_FILE);
    printf("fermeture de biceps. au revoir.\n");
    exit(0);
    return 0;
}

/* commande interne cd */
int CommandeCD(int n, char *p[]) {
    if (n < 2) {
        char *home = getenv("HOME");
        if (home) chdir(home);
    } else {
        if (chdir(p[1]) != 0) perror("cd");
    }
    return 1;
}

/* commande interne pwd */
int CommandePWD(int n, char *p[]) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) printf("%s\n", cwd);
    else perror("pwd");
    return 1;
}

/* commande interne vers */
int CommandeVERS(int n, char *p[]) {
    printf("biceps version 2.0 - protocole beuip v1\n");
    return 1;
}

/* gestion des sous-commandes start et stop */
int CommandeBEUIP(int n, char *p[]) {
    if (n < 2) {
        fprintf(stderr, "utilisation : beuip start pseudo | beuip stop\n");
        return 1;
    }

    /* cas du lancement */
    if (strcmp(p[1], "start") == 0) {
        if (n < 3) {
            fprintf(stderr, "erreur : pseudo manquant\n");
            return 1;
        }
        if (pid_beuip != -1) {
            fprintf(stderr, "erreur : serveur deja actif (pid %d)\n", pid_beuip);
            return 1;
        }
        pid_beuip = fork();
        if (pid_beuip == -1) {
            perror("fork");
        } else if (pid_beuip == 0) {
            execl("./servbeuip", "servbeuip", p[2], NULL);
            perror("execl");
            exit(1);
        } else {
            printf("serveur beuip lance avec le pid %d\n", pid_beuip);
        }
    } 
    /* cas de l arret */
    else if (strcmp(p[1], "stop") == 0) {
        if (pid_beuip == -1) {
            fprintf(stderr, "erreur : aucun serveur en cours d execution\n");
            return 1;
        }
        /* envoi du signal d interruption au fils */
        if (kill(pid_beuip, SIGINT) == 0) {
            printf("signal d arret envoye au serveur (pid %d)\n", pid_beuip);
            /* attente de la fin du processus pour eviter les zombies */
            waitpid(pid_beuip, NULL, 0);
            pid_beuip = -1;
        } else {
            perror("kill");
        }
    }
    else {
        fprintf(stderr, "sous-commande inconnue : %s\n", p[1]);
    }
    return 1;
}


/* enregistrement des commandes internes */
void majComInt(void) {
    ajouteCom("exit", Sortie);
    ajouteCom("cd", CommandeCD);
    ajouteCom("pwd", CommandePWD);
    ajouteCom("vers", CommandeVERS);
    ajouteCom("beuip", CommandeBEUIP);
}

int main(int argc, char *argv[]) {
    char *ligne;
    char *prompt;
    char *commande_isolee;
    int i;

    read_history(HIST_FILE);
    majComInt();

    while (1) {
        prompt = creer_prompt();
        ligne = readline(prompt);
        free(prompt);

        if (ligne == NULL) {
            printf("\n");
            Sortie(0, NULL);
        }

        if (strlen(ligne) > 0) {
            add_history(ligne);
            char* ptr_ligne = ligne;
            
            /* decoupage par point virgule */
            while ((commande_isolee = strsep(&ptr_ligne, ";")) != NULL) {
                if (*commande_isolee != '\0') {
                    char *cmds_pipe[MAXPAR];
                    int nb_pipe = 0;
                    char *ptr_pipe = commande_isolee;
                    
                    /* decoupage par pipe */
                    while ((cmds_pipe[nb_pipe] = strsep(&ptr_pipe, "|")) != NULL) {
                        if (*cmds_pipe[nb_pipe] != '\0') nb_pipe++;
                    }
                    
                    if (nb_pipe == 1) {
                        analyseCom(cmds_pipe[0]);
                        if (NMots > 0) {
                            /* execution interne ou externe */
                            if (execComInt(NMots, Mots) == 0) {
                                execComExt(Mots);
                            }
                            for (i = 0; i < NMots; i++) {
                                if (Mots[i] != NULL) {
                                    free(Mots[i]);
                                    Mots[i] = NULL;
                                }
                            }
                        }
                    } else if (nb_pipe > 1) {
                        execPipeline(cmds_pipe, nb_pipe);
                    }
                }
            }
        }
        free(ligne);
    }
    return 0;
}
