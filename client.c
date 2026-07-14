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

int main (void)
{
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];
    char server_ip[64];

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
            int bytes_read = recv (sock_fd, buffer, BUF_SIZE - 1, 0);

            if (bytes_read <= 0)
            {
                printf ("\nServer disconnected. Exiting...\n");
                break;
            }

            printf ("\rServer: %s\nYou: ", buffer);
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

            send (sock_fd, buffer, strlen (buffer), 0);

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