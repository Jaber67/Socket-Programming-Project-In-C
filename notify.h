#ifndef NOTIFY_H
#define NOTIFY_H

#include "server.h"

// Scans a chat message for @username mentions and pings each
// matched, connected user with a notification.
void notify_mentions(Client *sender, const char *message);

#endif
