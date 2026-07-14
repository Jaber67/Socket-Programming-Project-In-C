#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8080
#define BUF_SIZE 1024

int main (void)
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof (client_addr);
    char buffer[BUF_SIZE];

    // STEP 1: socket toiri kora (IPv4, TCP)
    server_fd = socket (AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror ("socket failed");
        exit (EXIT_FAILURE);
    }

    // Ekhon port ke reuse korte dibo, tai bind error ashbe na re-run korle
    int opt = 1;
    setsockopt (server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));

    // STEP 2: address struct set kora
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // sob interface theke connection nibe (LAN IP shoho)
    server_addr.sin_port = htons (PORT);

    // STEP 3: bind — socket ke ei IP+port er shathe attach kora
    if (bind (server_fd, (struct sockaddr *) &server_addr, sizeof (server_addr)) < 0)
    {
        perror ("bind failed");
        close (server_fd);
        exit (EXIT_FAILURE);
    }

    // STEP 4: listen — connection er jonno wait kora shuru
    if (listen (server_fd, 1) < 0)
    {
        perror ("listen failed");
        close (server_fd);
        exit (EXIT_FAILURE);
    }

    printf ("SERVER STARTED. Waiting for a client on port %d...\n", PORT);

    // STEP 5: accept — client connect korle eta ekta notun socket fd dey (client_fd)
    client_fd = accept (server_fd, (struct sockaddr *) &client_addr, &client_len);
    if (client_fd < 0)
    {
        perror ("accept failed");
        close (server_fd);
        exit (EXIT_FAILURE);
    }

    printf ("CLIENT CONNECTED from %s\n", inet_ntoa (client_addr.sin_addr));
    printf ("Type your message and press Enter. Type 'exit' to quit.\n\n");
    fflush (stdout);  // IMPORTANT: fork() er age buffer flush kora hocche
                       // noile ei buffered text child process e o thakbe ar duitai print korbe (duplicate)

    // STEP 6: fork() diye 2 ta process banabo
    // child process   -> khali receive kore print korbe
    // parent process  -> khali keyboard theke porbe ar send korbe
    pid_t pid = fork ();

    if (pid < 0)
    {
        perror ("fork failed");
        close (client_fd);
        close (server_fd);
        exit (EXIT_FAILURE);
    }

    if (pid == 0)
    {
        // ---- CHILD PROCESS: receiver ----
        while (1)
        {
            memset (buffer, 0, BUF_SIZE);
            int bytes_read = recv (client_fd, buffer, BUF_SIZE - 1, 0);

            if (bytes_read <= 0)
            {
                printf ("\nClient disconnected. Exiting...\n");
                break;
            }

            printf ("\rClient: %s\nYou: ", buffer);
            fflush (stdout);
        }
        close (client_fd);
        exit (0);
    }
    else
    {
        // ---- PARENT PROCESS: sender ----
        while (1)
        {
            printf ("You: ");
            fflush (stdout);

            memset (buffer, 0, BUF_SIZE);
            if (fgets (buffer, BUF_SIZE, stdin) == NULL)
                break;

            // fgets newline shoho rakhe, seta kete felbo
            buffer[strcspn (buffer, "\n")] = '\0';

            send (client_fd, buffer, strlen (buffer), 0);

            if (strcmp (buffer, "exit") == 0)
            {
                printf ("Exiting chat...\n");
                break;
            }
        }

        // Parent ber hoye gele child process ke o kill kore dibo
        kill (pid, SIGKILL);
        close (client_fd);
        close (server_fd);
    }

    return 0;
}