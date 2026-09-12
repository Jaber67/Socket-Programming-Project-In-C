#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUF_SIZE 1024
#define USERNAME_SIZE 32

// Buffers raw bytes from the socket so incoming messages can be read one
// line (terminated by '\n') at a time, matching how the server frames
// its sends. Without this, two messages arriving close together (e.g. a
// join notice while you're mid-type) can land in the same recv() call
// and get printed as one garbled blob.
typedef struct
{
    char buf[BUF_SIZE * 2];
    int len;
} LineReader;

// Returns 1 on success (line copied into out, newline/trailing '\r'
// stripped), 0 if the connection closed or errored.
int read_line (int sock, LineReader *lr, char *out, size_t out_size)
{
    while (1)
    {
        char *newline = memchr (lr->buf, '\n', lr->len);

        if (newline != NULL)
        {
            int line_len = newline - lr->buf;
            int copy_len = line_len;

            if ((size_t) copy_len >= out_size)
            {
                copy_len = out_size - 1;
            }

            memcpy (out, lr->buf, copy_len);
            out[copy_len] = '\0';

            if (copy_len > 0 && out[copy_len - 1] == '\r')
            {
                out[copy_len - 1] = '\0';
            }

            int consumed = line_len + 1;
            memmove (lr->buf, lr->buf + consumed, lr->len - consumed);
            lr->len -= consumed;

            return 1;
        }

        if ((size_t) lr->len >= sizeof (lr->buf) - 1)
        {
            int copy_len = lr->len;

            if ((size_t) copy_len >= out_size)
            {
                copy_len = out_size - 1;
            }

            memcpy (out, lr->buf, copy_len);
            out[copy_len] = '\0';
            lr->len = 0;

            return 1;
        }

        int n = recv (sock, lr->buf + lr->len, sizeof (lr->buf) - lr->len - 1, 0);

        if (n <= 0)
        {
            return 0;
        }

        lr->len += n;
    }
}

int main (void)
{
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];
    char server_ip[64];
    char username[USERNAME_SIZE];

    // Taking the IP address from the server
    printf ("Enter server IP address (e.g. 192.168.0.105): ");
    fgets (server_ip, sizeof (server_ip), stdin);
    server_ip[strcspn (server_ip, "\n")] = '\0';

    // CREATING THEEEEE SOCKETTTTT
    sock_fd = socket (AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        perror ("socket failed");
        exit (EXIT_FAILURE);
    }

    // SETTING THE SERVER ADDRESS STRUCT
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons (PORT);

    if (inet_pton (AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        printf ("Invalid IP address format.\n");
        close (sock_fd);
        exit (EXIT_FAILURE);
    }

    // Connecting to the server
    printf ("Connecting to %s:%d ...\n", server_ip, PORT);
    if (connect (sock_fd, (struct sockaddr *) &server_addr, sizeof (server_addr)) < 0)
    {
        perror ("connect failed");
        close (sock_fd);
        exit (EXIT_FAILURE);
    }

    printf ("CONNECTED TO SERVER.\n");

    // Ask for and send the username before anything else.
    // Must match server's is_valid_username(): letters, digits, underscore only.
    // Keep retrying until the server responds with "OK:".
    LineReader reader = {0};

    while (1)
    {
        printf ("Enter your username: ");
        fgets (username, sizeof (username), stdin);
        username[strcspn (username, "\n")] = '\0';

        char username_line[USERNAME_SIZE + 1];
        snprintf (username_line, sizeof (username_line), "%s\n", username);
        send (sock_fd, username_line, strlen (username_line), 0);

        memset (buffer, 0, BUF_SIZE);

        if (!read_line (sock_fd, &reader, buffer, BUF_SIZE))
        {
            printf ("Server disconnected.\n");
            close (sock_fd);
            exit (EXIT_FAILURE);
        }

        if (strncmp (buffer, "OK:", 3) == 0)
        {
            printf ("%s", buffer + 3);
            break;
        }
        else if (strncmp (buffer, "ERR:", 4) == 0)
        {
            printf ("%s", buffer + 4);
        }
        else
        {
            printf ("%s\n", buffer);
        }
    }

    printf ("Type your message and press Enter. Type 'exit' to quit.\n\n");
    fflush (stdout);


    pid_t pid = fork ();

    if (pid < 0)
    {
        perror ("fork failed");
        close (sock_fd);
        exit (EXIT_FAILURE);
    }

    if (pid == 0)
    {

        while (1)
        {
            memset (buffer, 0, BUF_SIZE);

            if (!read_line (sock_fd, &reader, buffer, BUF_SIZE))
            {
                printf ("\nServer disconnected. Exiting...\n");
                break;
            }

            // Clear whatever's on the current line (e.g. a half-typed
            // "You: ..." prompt) before printing the incoming message,
            // then redraw the prompt underneath it.
            printf ("\r\033[K%s\nYou: ", buffer);
            fflush (stdout);
        }
        close (sock_fd);
        exit (0);
    }
    else
    {

        while (1)
        {
            printf ("You: ");
            fflush (stdout);

            memset (buffer, 0, BUF_SIZE);
            if (fgets (buffer, BUF_SIZE, stdin) == NULL)
                break;

            buffer[strcspn (buffer, "\n")] = '\0';

            char out_line[BUF_SIZE + 1];
            snprintf (out_line, sizeof (out_line), "%s\n", buffer);
            send (sock_fd, out_line, strlen (out_line), 0);

            if (strcmp (buffer, "exit") == 0)
            {
                printf ("Exiting chat...\n");
                break;
            }
        }

        kill (pid, SIGKILL);
        close (sock_fd);
    }

    return 0;
}