CC=gcc
CFLAGS=-std=gnu11 -Wall -lpthread -lm

all: quicksort datagen

datagen: datagen.c 
	$(CC) -o datagen datagen.c $(CFLAGS)

quicksort: quicksort.c util.o
	$(CC) -o quicksort quicksort.c util.o $(CFLAGS)

util.o:
	$(CC) -c -o util.o util.c $(CFLAGS)

clean:
	rm datagen quicksort *.o