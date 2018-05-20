CC=gcc

all : cc

cc :
	$(CC) main.c fs.c -Wall -o main

clean :
	rm main