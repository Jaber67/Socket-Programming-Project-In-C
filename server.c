#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "username.h"
#include "server.h"
#include "commands.h"
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080

Client *clients[MAX_CLIENTS];
int client_count = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t chat_log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pm_log_mutex = PTHREAD_MUTEX_INITIALIZER;

static void current_timestamp(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", local);
}

void log_chat(const char *message)
{
    char timestamp[32];
    current_timestamp(timestamp, sizeof(timestamp));

    pthread_mutex_lock(&chat_log_mutex);

    FILE *f = fopen(CHAT_LOG_FILE, "a");

    if (f != NULL)
    {
        fprintf(f, "[%s] %s\n", timestamp, message);
        fclose(f);
    }

    pthread_mutex_unlock(&chat_log_mutex);
}

void log_pm(const char *sender, const char *recipient, const char *message)
{
    char timestamp[32];
    current_timestamp(timestamp, sizeof(timestamp));

    pthread_mutex_lock(&pm_log_mutex);

    FILE *f = fopen(PM_LOG_FILE, "a");

    if (f != NULL)
    {
        fprintf(f, "%s\t%s\t%s\t%s\n", sender, recipient, timestamp, message);
        fclose(f);
    }

    pthread_mutex_unlock(&pm_log_mutex);
}

int get_pm_history(const char *userA, const char *userB, char *out, size_t out_size)
{
    pthread_mutex_lock(&pm_log_mutex);

    FILE *f = fopen(PM_LOG_FILE, "r");

    if (f == NULL)
    {
        pthread_mutex_unlock(&pm_log_mutex);
        return 0;
    }

    out[0] = '\0';
    size_t used = 0;
    int found_any = 0;
    char line[BUFFER_SIZE + USERNAME_SIZE * 2 + 64];

    while (fgets(line, sizeof(line), f) != NULL)
    {
        char sender[USERNAME_SIZE];
        char recipient[USERNAME_SIZE];
        char timestamp[32];
        char *rest;

        char *tab1 = strchr(line, '\t');
        if (tab1 == NULL) continue;
        *tab1 = '\0';

        strncpy(sender, line, sizeof(sender) - 1);
        sender[sizeof(sender) - 1] = '\0';

        char *tab2 = strchr(tab1 + 1, '\t');
        if (tab2 == NULL) continue;
        *tab2 = '\0';

        strncpy(recipient, tab1 + 1, sizeof(recipient) - 1);
        recipient[sizeof(recipient) - 1] = '\0';

        char *tab3 = strchr(tab2 + 1, '\t');
        if (tab3 == NULL) continue;
        *tab3 = '\0';

        strncpy(timestamp, tab2 + 1, sizeof(timestamp) - 1);
        timestamp[sizeof(timestamp) - 1] = '\0';

        rest = tab3 + 1;

        int this_pair =
            (strcmp(sender, userA) == 0 && strcmp(recipient, userB) == 0) ||
            (strcmp(sender, userB) == 0 && strcmp(recipient, userA) == 0);

        if (!this_pair)
            continue;

        found_any = 1;

        char formatted[BUFFER_SIZE + USERNAME_SIZE * 2 + 64];

        int written = snprintf(
            formatted,
            sizeof(formatted),
            "[%s] %s: %s",
            timestamp,
            sender,
            rest
        );

        if (written < 0)
            continue;

        size_t write_len = (size_t)written;

        if (used + write_len >= out_size - 1)
            break;

        memcpy(out + used, formatted, write_len);
        used += write_len;
        out[used] = '\0';
    }

    fclose(f);
    pthread_mutex_unlock(&pm_log_mutex);

    return found_any;
}

int get_recent_chat_history(char *out, size_t out_size)
{
    pthread_mutex_lock(&chat_log_mutex);

    FILE *f = fopen(CHAT_LOG_FILE, "r");

    if (f == NULL)
    {
        pthread_mutex_unlock(&chat_log_mutex);
        out[0] = '\0';
        return 0;
    }

    static const size_t LINE_CAP = BUFFER_SIZE + USERNAME_SIZE + 32;

    char lines[HISTORY_MAX_LINES][BUFFER_SIZE + USERNAME_SIZE + 32];
    long total = 0;
    char linebuf[BUFFER_SIZE + USERNAME_SIZE + 32];

    while (fgets(linebuf, sizeof(linebuf), f) != NULL)
    {
        if (linebuf[0] == '\n')
            continue;

        strncpy(
            lines[total % HISTORY_MAX_LINES],
            linebuf,
            LINE_CAP - 1
        );

        lines[total % HISTORY_MAX_LINES][LINE_CAP - 1] = '\0';
        total++;
    }

    fclose(f);
    pthread_mutex_unlock(&chat_log_mutex);

    if (total == 0)
    {
        out[0] = '\0';
        return 0;
    }

    long start =
        (total > HISTORY_MAX_LINES) ?
        total - HISTORY_MAX_LINES : 0;

    long count = total - start;

    out[0] = '\0';
    size_t used = 0;

    for (long i = 0; i < count; i++)
    {
        long idx = (start + i) % HISTORY_MAX_LINES;
        size_t len = strlen(lines[idx]);

        if (used + len >= out_size - 1)
            break;

        memcpy(out + used, lines[idx], len);
        used += len;
        out[used] = '\0';
    }

    return 1;
}

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
                copy_len = out_size - 1;

            memcpy(out, lr->buf, copy_len);
            out[copy_len] = '\0';

            if (copy_len > 0 && out[copy_len - 1] == '\r')
                out[copy_len - 1] = '\0';

            int consumed = line_len + 1;

            memmove(
                lr->buf,
                lr->buf + consumed,
                lr->len - consumed
            );

            lr->len -= consumed;

            return 1;
        }

        if ((size_t)lr->len >= sizeof(lr->buf) - 1)
        {
            int copy_len = lr->len;

            if ((size_t)copy_len >= out_size)
                copy_len = out_size - 1;

            memcpy(out, lr->buf, copy_len);
            out[copy_len] = '\0';
            lr->len = 0;

            return 1;
        }

        int n = recv(
            sock,
            lr->buf + lr->len,
            sizeof(lr->buf) - lr->len - 1,
            0
        );

        if (n <= 0)
            return 0;

        lr->len += n;
    }
}

void broadcast_message(const char *message, int exclude_socket)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i]->socket != exclude_socket)
        {
            send(
                clients[i]->socket,
                message,
                strlen(message),
                0
            );
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

int send_to_user(const char *username, const char *message)
{
    int found = 0;

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (strcmp(clients[i]->username, username) == 0)
        {
            send(
                clients[i]->socket,
                message,
                strlen(message),
                0
            );

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

    char history[
        HISTORY_MAX_LINES *
        (BUFFER_SIZE + USERNAME_SIZE + 32)
    ];

    if (get_recent_chat_history(history, sizeof(history)))
    {
        char *header = "--- Recent chat history ---\n";

        send(client_socket, header, strlen(header), 0);
        send(client_socket, history, strlen(history), 0);

        char *footer = "--- End of history ---\n";

        send(client_socket, footer, strlen(footer), 0);
    }

    snprintf(
        out_msg,
        sizeof(out_msg),
        "*** %s has joined the chat ***\n",
        client->username
    );

    printf("%s", out_msg);
    broadcast_message(out_msg, client_socket);
    log_chat(out_msg);

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);

        if (!read_line(
                client_socket,
                &client->reader,
                buffer,
                BUFFER_SIZE))
        {
            break;
        }

        if (buffer[0] == '/')
        {
            CommandResult result =
                handle_command(client, buffer);

            if (result == COMMAND_HANDLED)
                continue;
        }

        printf("%s: %s\n", client->username, buffer);

        snprintf(
            out_msg,
            sizeof(out_msg),
            "%s: %s\n",
            client->username,
            buffer
        );

        broadcast_message(out_msg, client_socket);
        log_chat(out_msg);
    }

    snprintf(
        out_msg,
        sizeof(out_msg),
        "*** %s has left the chat ***\n",
        client->username
    );

    printf("%s", out_msg);
    broadcast_message(out_msg, client_socket);
    log_chat(out_msg);

    remove_client(client_socket);
    close(client_socket);

    printf(
        "Client disconnected: %s (socket %d)\n",
        client->username,
        client_socket
    );

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
            sizeof(server_address)) < 0)
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

        pthread_mutex_unlock(&clients_mutex);

        char username_buf[USERNAME_SIZE];
        int got_valid_username = 0;
        LineReader reader = {0};

        while (1)
        {
            memset(username_buf, 0, USERNAME_SIZE);

            if (!read_line(
                    client_socket,
                    &reader,
                    username_buf,
                    USERNAME_SIZE))
            {
                break;
            }

            if (!is_valid_username(username_buf))
            {
                char *message =
                    "ERR:Invalid username. Use only letters, "
                    "digits, and underscores. Try again: \n";

                send(
                    client_socket,
                    message,
                    strlen(message),
                    0
                );

                continue;
            }

            pthread_mutex_lock(&clients_mutex);

            int duplicate = 0;

            for (int i = 0; i < client_count; i++)
            {
                if (strcmp(
                        clients[i]->username,
                        username_buf) == 0)
                {
                    duplicate = 1;
                    break;
                }
            }

            pthread_mutex_unlock(&clients_mutex);

            if (duplicate)
            {
                char *message =
                    "ERR:Username already taken. Try again: \n";

                send(
                    client_socket,
                    message,
                    strlen(message),
                    0
                );

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

        strncpy(
            new_client->username,
            username_buf,
            USERNAME_SIZE - 1
        );

        new_client->username[USERNAME_SIZE - 1] = '\0';
        new_client->reader = reader;

        clients[client_count] = new_client;
        client_count++;

        char ok_msg[] = "OK:Username accepted.\n";

        send(
            client_socket,
            ok_msg,
            strlen(ok_msg),
            0
        );

        printf(
            "New client connected: %s (%s:%d)\n",
            new_client->username,
            inet_ntoa(client_address.sin_addr),
            ntohs(client_address.sin_port)
        );

        printf("Current clients: %d\n", client_count);

        pthread_mutex_unlock(&clients_mutex);

        pthread_t thread;

        if (pthread_create(
                &thread,
                NULL,
                handle_client,
                new_client) != 0)
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