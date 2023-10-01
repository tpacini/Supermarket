#define _POSIX_C_SOURCE 199506L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>

#include "director.h"
#include "glob.h"
#include "supermarket.h"

pthread_mutex_t configAccess;
Cashier_t **cashiers;

/* Parse S1 and S2 from the configuration file.
    Return 1 on success, 0 otherwise */
static unsigned int parseS(unsigned int *S1, unsigned int* S2)
{
    FILE* fp;
    char *buf1, *buf2, *tok;

    buf1 = (char *)malloc(MAX_LINE * sizeof(char));
    buf2 = (char *)malloc(MAX_LINE * sizeof(char));
    if (buf1 == NULL || buf2 == NULL)
    {
        perror("parseS: malloc");
        return 0;
    }

    pthread_mutex_lock(&configAccess);
    fp = fopen(CONFIG_FILENAME, "r");
    if (fp == NULL)
    {
        perror("parseS: fopen");
        pthread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return 0;
    }
    // Third row
    if (fseek(fp, 2L, SEEK_SET) == -1)
    {
        perror("parseS: fseek");
        pthread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return 0;
    }
    if (fread(buf1, sizeof(char), MAX_LINE, fp) == 0)
    {
        perror("parseS: fread");
        pthread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return 0;
    }
    // Fourth row
    if (fseek(fp, 3L, SEEK_SET) == -1)
    {
        perror("parseS: fseek");
        pthread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return 0;
    }
    if (fread(buf2, sizeof(char), MAX_LINE, fp) == 0)
    {
        perror("parseS: fread");
        pthread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return 0;
    }
    fclose(fp);
    pthread_mutex_unlock(&configAccess);

    tok = strtok(buf1, " ");
    while (tok != NULL)
    {
        // Next element is S1
        if (strcmp(tok, ":") == 0)
        {
            tok = strtok(NULL, " ");
            errno = 0;
            *S1 = strtoul(tok, NULL, 10);
            if (errno == EINVAL || errno == ERANGE)
            {
                fprintf(stderr, "parseS: strtoul: %d", errno);
                free(buf1);
                free(buf2);
                return 0;
            }
        }
        else
            tok = strtok(NULL, " ");
    }
    free(buf1);

    tok = strtok(buf2, " ");
    while (tok != NULL)
    {
        // Next element is S1
        if (strcmp(tok, ":") == 0)
        {
            tok = strtok(NULL, " ");
            errno = 0;
            *S2 = strtoul(tok, NULL, 10);
            if (errno == EINVAL || errno == ERANGE)
            {
                fprintf(stderr, "parseS: strtoul: %d", errno);
                free(buf2);
                return 0;
            }
        }
        else
            tok = strtok(NULL, " ");
    }
    free(buf2);

    if (DEBUG)
        printf("S1: %d, S2: %d\n", *S1, *S2);

    return 1;
}

/* Start a new Cashier thread. Return 0 on success,
    -1 otherwise */
static int openCashier(Cashier_t *ca)
{
    int ret;
    pthread_t thCa;

    if (ca == NULL)
    {
        perror("openCashier: ca is NULL");
        return -1;
    }

    pthread_mutex_lock(&ca->accessState);
    ca->open = 1;
    pthread_mutex_unlock(&ca->accessState);

    ret = pthread_create(&thCa, NULL, CashierP, ca);
    if (ret != 0)
    {
        perror("openCashier: pthread_create");
        return -1;
    }
    if (pthread_detach(thCa) != 0)
    {
        perror("openCashier: pthread_detach");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int sfd = -1, csfd = -1, optval, sigrecv = 0, ret, pid;
    char msg [2];
    sigset_t set;
    socklen_t optlen = sizeof(optval);
    struct timespec wait_signal;

    unsigned int K, C, E, T, S;
    unsigned int S1 = UINT_MAX, S2 = UINT_MAX, countS1, countS2;
    bool found = false;
    Cashier_t *closed_cashier = NULL;

    if (pthread_mutex_init(&configAccess, NULL) != 0)
    {
        perror("director: pthread_mutex_init");
        goto error;
    }

    if (argc != 7)
    {
        fprintf(stdout, "Usage: %s <K> <C> <E> <T> <P> <S>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    errno = 0;
    ret = 1;
    K = strtoul(argv[1], NULL, 10); // max. number of cashiers
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;
    C = strtoul(argv[2], NULL, 10); // max. number of customers inside supermarket
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;
    E = strtoul(argv[3], NULL, 10); // number of customers to enter supermarket simultaneously
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;
    T = strtoul(argv[4], NULL, 10); // max. time to buy products
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;
    S = strtoul(argv[6], NULL, 10); // time after customer looks for other cashiers
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;

    if (ret == 0)
    {
        perror("director: error converting arguments");
        exit(EXIT_FAILURE);
    }

    if (K <= 0 || C <= 0 || T <= 0 || S <= 0)
    {
        fprintf(stderr, "Parameters should be greater than 0\n");
        exit(EXIT_FAILURE);
    }
    if (!(E > 0 && E < C))
    {
        fprintf(stderr, "E should be greater than 0 and lower than C \n");
        exit(EXIT_FAILURE);
    }
   
    if (parseS(&S1, &S2) == 0)
    {
        perror("director: parseS");
        goto error;
    }

    // Setup signal handler 
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    // Initialize variables
    if (pthread_mutex_init(&gateCustomers, NULL) != 0)
    {
        perror("director: pthread_mutex_init");
        goto error;
    }
    if (pthread_cond_init(&exitCustomers, NULL) != 0)
    {
        perror("director: pthread_cond_init");
        goto error;
    }

    pthread_mutex_lock(&gateCustomers);
    gateClosed = true;
    pthread_mutex_unlock(&gateCustomers);

    // Execute Supermarket
    pid = fork();
    if (pid == 0)
    {
        if (execve(SUPMRKT_EXEC_PATH, argv, NULL) == -1)
        {
            perror("director: execve");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }

    // Create the socket to communicate with supermarket process
    sfd = socket(AF_UNIX, SOCK_STREAM, 0); // ip protocol
    if (sfd == -1)
    {
        perror("director: socket");
        goto error;
    }

    // Keep alive to ensure no unlimited waiting on write/read
    optval = 1; // enable option
    if (setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
    {
        perror("director: setsockopt");
        goto error;
    }

    // Bind and wait for a connection with the client
    if (bind(sfd, NULL, 0) == -1)
    {
        perror("director: bind"); // INVALID ARGUMENT [TO FIX]
        goto error;
    }
    if (listen(sfd, 1) == -1)
    {
        perror("director: listen");
        goto error;
    }
    csfd = accept(sfd, NULL, NULL);
    if (csfd == -1)
    {
        perror("director: accept");
        goto error;
    }

    if (DEBUG)
        printf("Socket accepted!\n");

    // Wait for a signal (BLOCKING) for 150 ms, repeatedly
    wait_signal.tv_sec = 0;
    wait_signal.tv_nsec = 150 * 1000000; // 150 ms
    countS1 = 0;
    countS2 = 0;
    while (sigrecv != SIGHUP || sigrecv != SIGQUIT)
    {
        errno = 0;
        if ((sigrecv = sigtimedwait(&set, NULL, 
                                        &wait_signal)) <= 0)
        {
            /* After 150ms if no signal arrived, check
                cashiers' situation */
            if (sigrecv == -1 && errno == EAGAIN && cashiers != NULL)
            {
                for (int i = 0; i < K; i++)
                {
                    pthread_mutex_lock(&cashiers[i]->accessState);
                    /* Cashier is closed and the conditions have
                        been met -> open cashier */
                    if (!(&cashiers[i]->open) && found)
                    {
                        ret = openCashier(cashiers[i]);
                        pthread_mutex_unlock(&cashiers[i]->accessState);
                        if (ret == -1)
                            goto error;
                        break;
                    }
                    /* Cashier is closed and the conditions have
                        not been satisfied yet -> save cashier for
                        when conditions will be satisfied */
                    else if (!(&cashiers[i]->open))
                    {
                        closed_cashier = cashiers[i];
                        pthread_mutex_unlock(&cashiers[i]->accessState);
                        continue;
                    }
                    else if (found)
                    {
                        pthread_mutex_unlock(&cashiers[i]->accessState);
                        continue;
                    }
                    pthread_mutex_unlock(&cashiers[i]->accessState);

                    pthread_mutex_lock(&cashiers[i]->accessQueue);
                    if (*(&cashiers[i]->queueCustomers->qlen) <= 1)
                        countS1 += 1;
                    if (*(&cashiers[i]->queueCustomers->qlen) >= S2)
                        countS2 += 1;
                    pthread_mutex_unlock(&cashiers[i]->accessQueue);

                    // Conditions satisfied, open a cashier
                    if (countS1 >= S1 || countS2 >= 1)
                    {
                        found = true;
                        if (closed_cashier != NULL) 
                        {
                            ret = openCashier(closed_cashier);
                            if (ret == -1)
                                goto error;
                            break;
                        }
                    }
                }
            }
            else
            {
                fprintf(stderr, "sigtimedwait: %d", errno);
                goto error;
            }
        }
    }

    if (DEBUG)
        printf("Received signal %d, sending signal to supermarket\n", sigrecv);

    // Send signal received to supermarket
    sprintf(msg, "%d", sigrecv);
    if (write(csfd, msg, strlen(msg)) == -1)
    {
        perror("director: write");
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

    if (DEBUG)
        printf("Closing Director...\n");

    close(sfd);
    return 0;

error:

    // Close supermarket, sending a SIGQUIT to it
    sprintf(msg, "%d", SIGQUIT);
    if (write(csfd, msg, strlen(msg)) == -1)
    {
        perror("director: write");
        exit(EXIT_FAILURE);
    }

    // Wait that the supermarket closes
    while (true)
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

    // close supermarket
    //kill(pid, SIGKILL);

    if (DEBUG)
        printf("Closing Director...\n");

    close(sfd);
    return -1;
}