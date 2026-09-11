CC = gcc
CFLAGS = -Wall -pthread

all: server client

server: server.c username.c username.h
	$(CC) $(CFLAGS) -o server server.c username.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client