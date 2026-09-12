CC = gcc
CFLAGS = -Wall -pthread

all: server client

server: server.c commands.c username.c username.h server.h commands.h
	$(CC) $(CFLAGS) -o server server.c commands.c username.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client