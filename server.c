#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "username.h"
#include <arpa/inet.h>
#include<pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

typedef struct
{
    int socket;
    char username[USERNAME_SIZE];
} Client;

Client *clients[MAX_CLIENTS];
int client_count=0;

pthread_mutex_t clients_mutex= PTHREAD_MUTEX_INITIALIZER;
void *handle_client(void *arg)
{
    Client *client=(Client *)arg;
    int client_socket=client->socket;
    char buffer[BUFFER_SIZE];
    
    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);

        int bytes_received = recv(
            client_socket,
            buffer,
            BUFFER_SIZE - 1,
            0
        );

        if (bytes_received <= 0)
        {
            break;
        }

        buffer[bytes_received] = '\0';

        printf("Client %d: %s", client_socket, buffer);

        pthread_mutex_lock(&clients_mutex);

        for (int i = 0; i < client_count; i++)
        {
            if (clients[i]->socket != client_socket)
            {
                send(
                    clients[i],
                    buffer,
                    strlen(buffer),
                    0
                );
            }
        }

        pthread_mutex_unlock(&clients_mutex);
    }

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i] == client_socket)
        {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    close(client_socket);

    printf("Client disconnected: %d\n", client_socket);

    return NULL;
}

int main()
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

    setsockopt(
        server_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if (bind(
        server_socket,
        (struct sockaddr *)&server_address,
        sizeof(server_address)
    ) < 0)
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

        int client_socket = accept(
            server_socket,
            (struct sockaddr *)&client_address,
            &client_length
        );

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

            send(
                client_socket,
                message,
                strlen(message),
                0
            );

            close(client_socket);
            continue;
        }

        clients[client_count] = client_socket;
        client_count++;

        printf(
            "New client connected: %s:%d\n",
            inet_ntoa(client_address.sin_addr),
            ntohs(client_address.sin_port)
        );

        printf("Current clients: %d\n", client_count);

        pthread_mutex_unlock(&clients_mutex);

        int *socket_ptr = malloc(sizeof(int));

        if (socket_ptr == NULL)
        {
            perror("Memory allocation failed");
            close(client_socket);
            continue;
        }

        *socket_ptr = client_socket;

        pthread_t thread;

        if (pthread_create(
            &thread,
            NULL,
            handle_client,
            socket_ptr
        ) != 0)
        {
            perror("Thread creation failed");

            pthread_mutex_lock(&clients_mutex);

            client_count--;

            pthread_mutex_unlock(&clients_mutex);

            close(client_socket);
            free(socket_ptr);

            continue;
        }

        pthread_detach(thread);
    }

    close(server_socket);

    return 0;
}