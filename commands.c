#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "server.h"
#include "commands.h"

static void handle_help(Client *client)
{
    char *help_message =
        "Available commands:\n"
        "/help\n"
        "/list\n"
        "/msg username message\n"
        "/broadcast message\n"
        "/history username\n";

    send(client->socket, help_message, strlen(help_message), 0);
}

static void handle_history(Client *client, const char *other_username);

static void handle_msg(Client *client, char *args)
{
    char *space = strchr(args, ' ');

    if (space == NULL)
    {
        char *err = "ERR:Usage: /msg username message\n";
        send(client->socket, err, strlen(err), 0);
        return;
    }

    *space = '\0';
    char *target_username = args;
    char *msg_text = space + 1;

    if (strlen(msg_text) == 0)
    {
        char *err = "ERR:Message cannot be empty.\n";
        send(client->socket, err, strlen(err), 0);
        return;
    }

    if (strcmp(msg_text, "chathistory") == 0)
    {
        handle_history(client, target_username);
        return;
    }

    if (strcmp(target_username, client->username) == 0)
    {
        char *err = "ERR:You can't message yourself.\n";
        send(client->socket, err, strlen(err), 0);
        return;
    }

    char private_msg[BUFFER_SIZE + USERNAME_SIZE + 16];
    snprintf(private_msg, sizeof(private_msg),
            "[PM from %s]: %s\n", client->username, msg_text);

    if (send_to_user(target_username, private_msg))
    {
        char confirm[BUFFER_SIZE + USERNAME_SIZE + 16];
        snprintf(confirm, sizeof(confirm),
                "[PM to %s]: %s\n", target_username, msg_text);
        send(client->socket, confirm, strlen(confirm), 0);

        log_pm(client->username, target_username, msg_text);
    }
    else
    {
        char err[BUFFER_SIZE];
        snprintf(err, sizeof(err),
                "ERR:User '%s' not found.\n", target_username);
        send(client->socket, err, strlen(err), 0);
    }
}

static void handle_broadcast(Client *client, char *msg_text)
{
    if (strlen(msg_text) == 0)
    {
        char *err = "ERR:Usage: /broadcast message\n";
        send(client->socket, err, strlen(err), 0);
        return;
    }

    printf("[broadcast] %s: %s\n", client->username, msg_text);

    char out_msg[BUFFER_SIZE + USERNAME_SIZE + 16];
    snprintf(out_msg, sizeof(out_msg),
            "[broadcast] %s: %s\n", client->username, msg_text);

    broadcast_message(out_msg, -1);
    log_chat(out_msg);
}

static void handle_history(Client *client, const char *other_username)
{
    if (strlen(other_username) == 0)
    {
        char *err = "ERR:Usage: /history username\n";
        send(client->socket, err, strlen(err), 0);
        return;
    }

    char history[BUFFER_SIZE * 8];

    if (get_pm_history(client->username, other_username, history, sizeof(history)) && strlen(history) > 0)
    {
        char header[USERNAME_SIZE + 32];
        snprintf(header, sizeof(header),
                "--- History with %s ---\n", other_username);
        send(client->socket, header, strlen(header), 0);
        send(client->socket, history, strlen(history), 0);
    }
    else
    {
        char *msg = "No messages found.\n";
        send(client->socket, msg, strlen(msg), 0);
    }
}

CommandResult handle_command(Client *client, const char *buffer)
{
    if (strcmp(buffer, "/help") == 0)
    {
        handle_help(client);
        return COMMAND_HANDLED;
    }

    if (strcmp(buffer, "/list") == 0)
    {
        char list_message[BUFFER_SIZE];
        int offset = 0;

        offset += snprintf(list_message, sizeof(list_message),
                           "Connected users:\n");

        pthread_mutex_lock(&clients_mutex);

        for (int i = 0; i < client_count; i++)
        {
            offset += snprintf(list_message + offset,
                               sizeof(list_message) - offset,
                               "%s\n",
                               clients[i]->username);
        }

        pthread_mutex_unlock(&clients_mutex);

        send(client->socket, list_message, strlen(list_message), 0);
        return COMMAND_HANDLED;
    }

    if (strncmp(buffer, "/msg ", 5) == 0)
    {
        char args[BUFFER_SIZE];
        strncpy(args, buffer + 5, sizeof(args) - 1);
        args[sizeof(args) - 1] = '\0';

        handle_msg(client, args);
        return COMMAND_HANDLED;
    }

    if (strncmp(buffer, "/broadcast ", 11) == 0)
    {
        handle_broadcast(client, (char *)buffer + 11);
        return COMMAND_HANDLED;
    }

    if (strncmp(buffer, "/history ", 9) == 0)
    {
        handle_history(client, buffer + 9);
        return COMMAND_HANDLED;
    }

    if (strcmp(buffer, "/history") == 0)
    {
        char *err = "ERR:Usage: /history username\n";
        send(client->socket, err, strlen(err), 0);
        return COMMAND_HANDLED;
    }

    return COMMAND_NOT_MATCHED;
}