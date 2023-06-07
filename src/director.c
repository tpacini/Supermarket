#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>

#include "glob.h"

long to_long(char* to_convert)
{
    char *endptr;
    long val;

    errno = 0; /* To distinguish success/failure after call */
    val = strtol(to_convert, &endptr, 10);

    /* Check for various possible errors. */

    if (errno != 0)
    {
        perror("strtol");
        exit(EXIT_FAILURE);
    }

    if (endptr == to_convert)
    {
        fprintf(stderr, "No digits were found\n");
        exit(EXIT_FAILURE);
    }

    free(endptr);
    return val;
}



int main(int argc, char *argv[])
{
    int sfd, csfd, optval, sigrecv = 0;
    char msg [2];
    sigset_t set;
    socklen_t optlen = sizeof(optval);

    // Create the socket to communicate with supermarket process
    sfd = socket(AF_UNIX, SOCK_STREAM, 0); // ip protocol
    if (sfd == -1)
    {
        perror("socket");
        goto error;
    }

    // Keep alive to ensure no unlimited waiting on write/read
    optval = 1; // enable option
    if (setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
    {
        perror("setsockopt");
        goto error;
    }

    // Bind and wait for a connection with the client
    if (bind(sfd, NULL, 0) == -1)
    {
        perror("bind");
        goto error;
    }
    if (listen(sfd, 1) == -1)
    {
        perror("listen");
        goto error;
    }
    csfd = accept(sfd, NULL, NULL);
    if (csfd == -1)
    {
        perror("accept");
        goto error;
    }

    // Setup signal handler 
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    /* ------------------------------- */
    // pthread_mutex_t gateCustomers = PTHREAD_MUTEX_INITIALIZER;
    // pthread_cond_t exitCustomers = PTHREAD_COND_INITIALIZER;

    // Execute Supermarket PROCESS

    // pthread_create(&thNotifier, NULL, Notifier, NULL)

    // Start Director

    // set gateClosed

    // Start Supermarket
    /* ------------------------------- */

    // Wait for a signal (BLOCKING)
    while (sigrecv != SIGHUP || sigrecv != SIGQUIT)
    {
        if (sigwait(&set, &sigrecv) != 0)
        {
            perror("sigwait");
            goto error;
        }
    }

    // Send signal received to supermarket
    sprintf(msg, "%d", sigrecv);
    if (write(csfd, msg, strlen(msg)) == -1)
    {
        perror("write");
        goto error;
    }

    // Wait that the supermarket closes
    while(true)
    {
        if (write(csfd, msg, strlen(msg)) == -1 && errno == EPIPE)
        {
            printf("Supermarket closed!\n");
            break;
        }
        else
        {
            sleep(1);
        }
    }

    // CAREFUL TO SIGPIPE

    close(sfd);
    return 0;

error:

    // close supermarket
    

    close(sfd);
}