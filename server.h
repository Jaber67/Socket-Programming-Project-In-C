#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>
#include <pthread.h>
#include "username.h"

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

// Buffers raw bytes from a socket so messages can be read one line
// (terminated by '\n') at a time, instead of assuming a single recv()
// call lines up with a single message. TCP gives no such guarantee -
// multiple sends can arrive in one recv(), or one send can be split
// across several recv() calls.
typedef struct
{
    char buf[BUFFER_SIZE * 2];
    int len;
} LineReader;

typedef struct
{
    int socket;
    char username[USERNAME_SIZE];
    LineReader reader;
} Client;

extern Client *clients[MAX_CLIENTS];
extern int client_count;
extern pthread_mutex_t clients_mutex;

// Reads the next '\n'-terminated line from the socket into out
// (null-terminated, newline and any trailing '\r' stripped).
// Returns 1 on success, 0 if the connection closed or errored.
int read_line(int sock, LineReader *lr, char *out, size_t out_size);

// Sends a message to every connected client except (optionally) one.
// Pass exclude_socket = -1 to send to everyone.
void broadcast_message(const char *message, int exclude_socket);

// Sends a message to exactly one client by username.
// Returns 1 if the user was found and the message was sent, 0 otherwise.
int send_to_user(const char *username, const char *message);

// Removes a client from the connected-clients list and frees it.
void remove_client(int client_socket);

#endif