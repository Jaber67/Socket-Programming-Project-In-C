#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "username.h"
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

typedef struct
{
    int socket;
    char username[USERNAME_SIZE];
} Client;

Client *clients[MAX_CLIENTS];
int client_count = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

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

        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received <= 0)
        {
            break;
        }

        buffer[bytes_received] = '\0';

        // Check if the message is /help
        if (strcmp(buffer, "/help") == 0)
        {
            char *help_message =
                "Available commands:\n"
                "/help\n"
                "/list\n"
                "/whoami\n"
                "/msg username message\n"
                "/broadcast message\n"
                "/quit\n";

            send(client_socket, help_message,
                strlen(help_message), 0);
        }
        else
        {
            // Normal chat message
            printf("%s: %s\n", client->username, buffer);

            snprintf(out_msg, sizeof(out_msg),
                    "%s: %s\n", client->username, buffer);

            broadcast_message(out_msg, client_socket);
        }
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

        while (1)
        {
            memset(username_buf, 0, USERNAME_SIZE);

            int name_bytes = recv(client_socket, username_buf, USERNAME_SIZE - 1, 0);

            if (name_bytes <= 0)
            {
                // Client disconnected while choosing a username.
                break;
            }

            username_buf[strcspn(username_buf, "\r\n")] = '\0';

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