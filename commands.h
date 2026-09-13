#ifndef COMMANDS_H
#define COMMANDS_H

#include "server.h"

typedef enum
{
    COMMAND_NOT_MATCHED,
    COMMAND_HANDLED
} CommandResult;

// Command handler
CommandResult handle_command(Client *client, const char *buffer);

#endif