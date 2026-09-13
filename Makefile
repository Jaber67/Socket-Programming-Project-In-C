CC      := gcc
CFLAGS  := -Wall -Wextra -g
LDLIBS  := -lpthread

.PHONY: all clean

all: server client

server: server.c username.c commands.c username.h server.h commands.h
	$(CC) $(CFLAGS) -o server server.c username.c commands.c $(LDLIBS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client