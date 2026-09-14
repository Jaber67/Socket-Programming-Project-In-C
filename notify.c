
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "server.h"
#include "notify.h"

void notify_mentions(Client *sender, const char *message)
{
    int i = 0;

    while (message[i] != '\0')
    {
        /*
         * Check whether the current character is '@'
         * and whether it starts a new word.
         */
        if (message[i] == '@' &&
            (i == 0 || isspace((unsigned char)message[i - 1])))
        {
            char target[USERNAME_SIZE];
            int j = i + 1;
            int len = 0;

            /*
             * Extract username characters after '@'.
             * Valid username characters:
             * letters, digits, underscore
             */
            while (message[j] != '\0' &&
                   (isalnum((unsigned char)message[j]) ||
                    message[j] == '_'))
            {
                if (len < USERNAME_SIZE - 1)
                {
                    target[len++] = message[j];
                }

                j++;
            }

            target[len] = '\0';

            /*
             * Send notification only if:
             * 1. Username is not empty.
             * 2. Target is not the sender.
             */
            if (len > 0 &&
                strcmp(target, sender->username) != 0)
            {
                char notice[BUFFER_SIZE + USERNAME_SIZE + 32];

                snprintf(notice, sizeof(notice),
                         "*** %s mentioned you: %s ***\n",
                         sender->username, message);

                send_to_user(target, notice);
            }

            /*
             * Continue scanning after the username.
             */
            i = j;
        }
        else
        {
            i++;
        }
    }
}