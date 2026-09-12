#ifndef COMMANDS_H
#define COMMANDS_H

#include "server.h"

typedef enum
{
    COMMAND_NOT_MATCHED, // buffer wasn't a recognized command; caller should
                         // treat it as a normal chat message
    COMMAND_HANDLED      // command was recognized and fully handled
} CommandResult;

// Checks whether `buffer` is one of the recognized "/..." commands for
// `client`, and if so, handles it completely (including any responses
// sent back to the client). Returns COMMAND_NOT_MATCHED if `buffer`
// isn't a recognized command, so the caller can fall back to treating
// it as a normal chat message.
CommandResult handle_command(Client *client, const char *buffer);

#endif