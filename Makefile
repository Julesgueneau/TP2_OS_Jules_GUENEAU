# variables de compilation
CC = cc
CFLAGS = -Wall
LDFLAGS = -lreadline

# cibles principales
all: biceps servbeuip clibeuip servudp cliudp

# regle pour compiler la gestion des commandes du tp precedent
gescom.o : gescom.c gescom.h
	cc -Wall -c gescom.c

# programme principal biceps (lie gescom et creme)
biceps: biceps.c creme.o gescom.o
	$(CC) $(CFLAGS) -o biceps biceps.c creme.o gescom.o $(LDFLAGS)

# serveur beuip (avec traces de debug activees)
servbeuip: servbeuip.c creme.o
	$(CC) $(CFLAGS) -DTRACE -o servbeuip servbeuip.c creme.o

# client de pilotage beuip
clibeuip: clibeuip.c creme.o
	$(CC) $(CFLAGS) -o clibeuip clibeuip.c creme.o

# compilation de la bibliothèque reseau
creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -c creme.c

# programmes udp de l'etape 1
servudp: servudp.c
	$(CC) $(CFLAGS) -o servudp servudp.c

cliudp: cliudp.c
	$(CC) $(CFLAGS) -o cliudp cliudp.c

# nettoyage des fichiers objets et executables
clean:
	rm -f *.o biceps servbeuip clibeuip servudp cliudp
