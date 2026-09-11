#include <string.h>
#include <ctype.h>

#include "username.h"

int is_valid_username(const char *username)
{
    int length = strlen(username);

    if (length == 0 || length >= USERNAME_SIZE)
    {
        return 0;
    }

    for (int i = 0; i < length; i++)
    {
        if (!isalnum(username[i]) && username[i] != '_')
        {
            return 0;
        }
    }

    return 1;
}