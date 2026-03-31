/* en-tete de la librairie gescom */
#ifndef GESCOM_H
#define GESCOM_H

#define MAXPAR 10
#define NBMAXC 10

extern char *Mots[MAXPAR];
extern int NMots;

struct commande_interne {
    char *nom;
    int (*fonction)(int, char **);
};

char* copyString(char* s);
int analyseCom(char* b);
void ajouteCom(char *nom, int (*fonc)(int, char **));
int execComInt(int n, char **p);
int execComExt(char **p);
void execPipeline(char **cmds, int nb); /* nouvelle fonction pour les pipes */

#endif