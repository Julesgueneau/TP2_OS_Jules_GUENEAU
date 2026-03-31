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

/* stockage du pid du serveur beuip */
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

/* gestion de la sortie de biceps */
int Sortie(int n, char *p[]) {
    write_history(HIST_FILE);
    /* si un serveur tourne encore, on le ferme */
    if (pid_beuip != -1) {
        kill(pid_beuip, SIGUSR1);
        waitpid(pid_beuip, NULL, 0);
    }
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
    printf("biceps version 3.3 - gestion signaux beuip\n");
    return 1;
}

/* gestion du protocole beuip (start/stop) */
int CommandeBEUIP(int n, char *p[]) {
    if (n < 2) {
        fprintf(stderr, "usage : beuip start|stop|list|all|msg\n");
        return 1;
    }

    if (strcmp(p[1], "start") == 0) {
        if (n < 3) {
            fprintf(stderr, "erreur : pseudo manquant\n");
            return 1;
        }
        if (pid_beuip != -1) {
            fprintf(stderr, "serveur deja actif\n");
            return 1;
        }
        pid_beuip = fork();
        if (pid_beuip == 0) {
            execl("./servbeuip", "servbeuip", p[2], NULL);
            perror("execl");
            exit(1);
        }
    } 
    else if (strcmp(p[1], "stop") == 0) {
        if (pid_beuip != -1) {
            kill(pid_beuip, SIGUSR1);
            waitpid(pid_beuip, NULL, 0);
            pid_beuip = -1;
            printf("serveur arrete\n");
        }
    }
    /* ajout des commandes de pilotage via clibeuip */
    else if (strcmp(p[1], "list") == 0) {
        /* le client attend : code donnee */
        char *args[] = {"./clibeuip", "3", "presents", NULL};
        if (fork() == 0) {
            execv(args[0], args);
            exit(1);
        }
        wait(NULL);
    }
    else if (strcmp(p[1], "all") == 0 && n >= 3) {
        /* le client attend : code message */
        char *args[] = {"./clibeuip", "5", p[2], NULL};
        if (fork() == 0) {
            execv(args[0], args);
            exit(1);
        }
        wait(NULL);
    }
    else if (strcmp(p[1], "msg") == 0 && n >= 4) {
        /* le client attend : code destinataire message */
        char *args[] = {"./clibeuip", "4", p[2], p[3], NULL};
        if (fork() == 0) {
            execv(args[0], args);
            exit(1);
        }
        wait(NULL);
    }
    else {
        fprintf(stderr, "commande inconnue : %s\n", p[1]);
    }
    return 1;
}


/* table des commandes internes */
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
            
            while ((commande_isolee = strsep(&ptr_ligne, ";")) != NULL) {
                if (*commande_isolee != '\0') {
                    char *cmds_pipe[MAXPAR];
                    int nb_pipe = 0;
                    char *ptr_pipe = commande_isolee;
                    
                    while ((cmds_pipe[nb_pipe] = strsep(&ptr_pipe, "|")) != NULL) {
                        if (*cmds_pipe[nb_pipe] != '\0') nb_pipe++;
                    }
                    
                    if (nb_pipe == 1) {
                        analyseCom(cmds_pipe[0]);
                        if (NMots > 0) {
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
