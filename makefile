CC=gcc

all : cc

cc :
	$(CC) main.c fs.c -pedantic -o main -g

clean :
	rm main