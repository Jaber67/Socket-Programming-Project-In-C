#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "username.h"
#include "server.h"
#include "commands.h"
#include <arpa/inet.h>
#include <pthread.h>
#include "username.h"
#include "server.h"
#include "commands.h"
#include "notify.h"

#define PORT 8080

Client *clients[MAX_CLIENTS];
int client_count = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Reads the next '\n'-terminated line from the socket into out
// (null-terminated, newline and any trailing '\r' stripped).
// Returns 1 on success, 0 if the connection closed or errored.
int read_line(int sock, LineReader *lr, char *out, size_t out_size)
{
    while (1)
    {
        char *newline = memchr(lr->buf, '\n', lr->len);

        if (newline != NULL)
        {
            int line_len = newline - lr->buf;
            int copy_len = line_len;

            if ((size_t)copy_len >= out_size)
            {
                copy_len = out_size - 1;
            }

            memcpy(out, lr->buf, copy_len);
            out[copy_len] = '\0';

            if (copy_len > 0 && out[copy_len - 1] == '\r')
            {
                out[copy_len - 1] = '\0';
            }

            int consumed = line_len + 1; // include the '\n'
            memmove(lr->buf, lr->buf + consumed, lr->len - consumed);
            lr->len -= consumed;

            return 1;
        }

        if ((size_t)lr->len >= sizeof(lr->buf) - 1)
        {
            // Line too long for our buffer with no newline in sight;
            // flush what we have as a single line rather than overflow.
            int copy_len = lr->len;

            if ((size_t)copy_len >= out_size)
            {
                copy_len = out_size - 1;
            }

            memcpy(out, lr->buf, copy_len);
            out[copy_len] = '\0';
            lr->len = 0;

            return 1;
        }

        int n = recv(sock, lr->buf + lr->len, sizeof(lr->buf) - lr->len - 1, 0);

        if (n <= 0)
        {
            return 0;
        }

        lr->len += n;
    }
}

// Send a message to every client except (optionally) the sender.
// Pass exclude_socket = -1 to send to everyone.
void broadcast_message(const char *message, int exclude_socket)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i]->socket != exclude_socket)
        {
            send(clients[i]->socket, message, strlen(message), 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Send a message to exactly one client by username.
// Returns 1 if the user was found and the message was sent, 0 otherwise.
int send_to_user(const char *username, const char *message)
{
    int found = 0;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (strcmp(clients[i]->username, username) == 0)
        {
            send(clients[i]->socket, message, strlen(message), 0);
            found = 1;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    return found;
}

void remove_client(int client_socket)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i]->socket == client_socket)
        {
            free(clients[i]);
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg)
{
    Client *client = (Client *)arg;
    int client_socket = client->socket;
    char buffer[BUFFER_SIZE];
    char out_msg[BUFFER_SIZE + USERNAME_SIZE + 4];

    // Announce join to everyone else
    snprintf(out_msg, sizeof(out_msg), "*** %s has joined the chat ***\n", client->username);
    printf("%s", out_msg);
    broadcast_message(out_msg, client_socket);

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);

        if (!read_line(client_socket, &client->reader, buffer, BUFFER_SIZE))
        {
            break;
        }

        if (buffer[0] == '/')
        {
            CommandResult result = handle_command(client, buffer);

            if (result == COMMAND_HANDLED)
            {
                continue;
            }

            // COMMAND_NOT_MATCHED (e.g. an unrecognized "/whatever") falls
            // through and is broadcast as a normal chat message below.
        }

        // Normal chat message
        printf("%s: %s\n", client->username, buffer);

        snprintf(out_msg, sizeof(out_msg),
                "%s: %s\n", client->username, buffer);

        broadcast_message(out_msg, client_socket);
	notify_mentions(client, buffer);
    }

    // Announce leave to everyone else
    snprintf(out_msg, sizeof(out_msg), "*** %s has left the chat ***\n", client->username);
    printf("%s", out_msg);
    broadcast_message(out_msg, client_socket);

    remove_client(client_socket);

    close(client_socket);

    printf("Client disconnected: %s (socket %d)\n", client->username, client_socket);

    return NULL;
}

int main(void)
{
    int server_socket;
    struct sockaddr_in server_address;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("Bind failed");
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, MAX_CLIENTS) < 0)
    {
        perror("Listen failed");
        close(server_socket);
        return 1;
    }

    printf("=================================\n");
    printf("      Chat Server Started\n");
    printf("      Port: %d\n", PORT);
    printf("      Max Clients: %d\n", MAX_CLIENTS);
    printf("=================================\n");

    while (1)
    {
        struct sockaddr_in client_address;
        socklen_t client_length = sizeof(client_address);

        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_length);

        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);

        if (client_count >= MAX_CLIENTS)
        {
            pthread_mutex_unlock(&clients_mutex);

            char *message = "Server is full.\n";
            send(client_socket, message, strlen(message), 0);

            close(client_socket);
            continue;
        }

        pthread_mutex_unlock(&clients_mutex);

        // Receive and validate the username, giving the client repeated
        // chances to pick a valid, unused one instead of disconnecting them.
        char username_buf[USERNAME_SIZE];
        int got_valid_username = 0;
        LineReader reader = {0};

        while (1)
        {
            memset(username_buf, 0, USERNAME_SIZE);

            if (!read_line(client_socket, &reader, username_buf, USERNAME_SIZE))
            {
                // Client disconnected while choosing a username.
                break;
            }

            if (!is_valid_username(username_buf))
            {
                char *message = "ERR:Invalid username. Use only letters, digits, and underscores. Try again: ";
                send(client_socket, message, strlen(message), 0);
                continue;
            }

            pthread_mutex_lock(&clients_mutex);

            int duplicate = 0;
            for (int i = 0; i < client_count; i++)
            {
                if (strcmp(clients[i]->username, username_buf) == 0)
                {
                    duplicate = 1;
                    break;
                }
            }

            pthread_mutex_unlock(&clients_mutex);

            if (duplicate)
            {
                char *message = "ERR:Username already taken. Try again: ";
                send(client_socket, message, strlen(message), 0);
                continue;
            }

            got_valid_username = 1;
            break;
        }

        if (!got_valid_username)
        {
            close(client_socket);
            continue;
        }

        pthread_mutex_lock(&clients_mutex);

        Client *new_client = malloc(sizeof(Client));

        if (new_client == NULL)
        {
            pthread_mutex_unlock(&clients_mutex);
            perror("Memory allocation failed");
            close(client_socket);
            continue;
        }

        new_client->socket = client_socket;
        strncpy(new_client->username, username_buf, USERNAME_SIZE - 1);
        new_client->username[USERNAME_SIZE - 1] = '\0';
        new_client->reader = reader; // preserve any bytes already buffered

        clients[client_count] = new_client;
        client_count++;

        // Let the client know the username was accepted.
        char ok_msg[] = "OK:Username accepted.\n";
        send(client_socket, ok_msg, strlen(ok_msg), 0);

        printf(
            "New client connected: %s (%s:%d)\n",
            new_client->username,
            inet_ntoa(client_address.sin_addr),
            ntohs(client_address.sin_port)
        );

        printf("Current clients: %d\n", client_count);

        pthread_mutex_unlock(&clients_mutex);

        pthread_t thread;

        if (pthread_create(&thread, NULL, handle_client, new_client) != 0)
        {
            perror("Thread creation failed");
            remove_client(client_socket);
            close(client_socket);
            continue;
        }

        pthread_detach(thread);
    }

    close(server_socket);

    return 0;
}
