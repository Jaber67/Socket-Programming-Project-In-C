#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>
#include <pthread.h>
#include "username.h"

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

#define CHAT_LOG_FILE "chat_log.txt"
#define PM_LOG_FILE "pm_log.txt"
#define HISTORY_MAX_LINES 50

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
extern pthread_mutex_t chat_log_mutex;
extern pthread_mutex_t pm_log_mutex;

// Functions
int read_line(int sock, LineReader *lr, char *out, size_t out_size);

void broadcast_message(const char *message, int exclude_socket);

int send_to_user(const char *username, const char *message);

void remove_client(int client_socket);

void log_chat(const char *message);

void log_pm(const char *sender, const char *recipient, const char *message);

int get_pm_history(const char *userA, const char *userB,
                   char *out, size_t out_size);

int get_recent_chat_history(char *out, size_t out_size);

#endif