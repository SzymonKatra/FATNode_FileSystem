CC=gcc

all : cc

cc :
	$(CC) main.c fs.c -pedantic -o fs -g

clean :
	rm main