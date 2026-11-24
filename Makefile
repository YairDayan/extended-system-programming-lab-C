CC = gcc
CFLAGS = -g -Wall

all: mypipeline myshell looper

mypipeline: mypipeline.c
	$(CC) $(CFLAGS) -o mypipeline mypipeline.c

myshell: myshell.c LineParser.c LineParser.h
	$(CC) $(CFLAGS) -o myshell myshell.c LineParser.c

looper: looper.c
	$(CC) $(CFLAGS) -o looper looper.c

clean:
	rm -f mypipeline myshell looper

.PHONY: all clean
