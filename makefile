CC=gcc

all :
	$(CC) main.c fs.c -pedantic -o fs

debug : 
	$(CC) main.c fs.c -pedantic -o fs -g

clean :
	rm fs