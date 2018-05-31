CC=gcc

all : cc

cc :
	$(CC) main.c fs.c parser.c -pedantic -o fs -g

clean :
	rm main