#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "server.h"
#include "notify.h"

void notify_mentions(Client *sender, const char *message)
{
    char scan_buf[BUFFER_SIZE];
    strncpy(scan_buf, message, sizeof(scan_buf) - 1);
    scan_buf[sizeof(scan_buf) - 1] = '\0';

    char *token = strtok(scan_buf, " ");
    while (token != NULL)
    {
        if (token[0] == '@' && strlen(token) > 1)
        {
            char target[USERNAME_SIZE];
            strncpy(target, token + 1, sizeof(target) - 1);
            target[sizeof(target) - 1] = '\0';

            int len = strlen(target);
            while (len > 0 && !isalnum((unsigned char)target[len - 1]) && target[len - 1] != '_')
            {
                target[--len] = '\0';
            }

            if (len > 0 && strcmp(target, sender->username) != 0)
            {
                char notice[BUFFER_SIZE + USERNAME_SIZE + 32];
                snprintf(notice, sizeof(notice),
                         "*** %s mentioned you: %s ***\n",
                         sender->username, message);
                send_to_user(target, notice);
            }
        }
        token = strtok(NULL, " ");
    }
}
