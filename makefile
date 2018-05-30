CC=gcc

all : cc

cc :
	$(CC) main.c fs.c parser.c -pedantic -o main -g

clean :
	rm main