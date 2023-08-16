#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>

#include "glob.h"
#include "director.h"
#include "supermarket.h"

long to_long(char* to_convert)
{
    char *endptr;
    long val;

    errno = 0; /* To distinguish success/failure after call */
    val = strtol(to_convert, &endptr, 10);

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

unsigned int parseS(unsigned int *S1, unsigned int* S2)
{
    FILE* fp;
    unsigned char *buf1, *buf2, *tok;

    buf1 = (char *)malloc(MAX_LINE * sizeof(char));
    buf2 = (char *)malloc(MAX_LINE * sizeof(char));
    if (buf1 == NULL || buf2 == NULL)
    {
        perror("malloc");
        return 0;
    }

    pthread_mutex_lock(&configAccess);
    fp = fopen(CONFIG_FILENAME, 'r');
    if (fp == NULL)
    {
        perror("fopen");
        pthread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return 0;
    }

    if (fseek(fp, 2L, SEEK_SET) == -1)
    {
        perror("fseek");
        thread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return 0;
    }
    if (fread(buf1, sizeof(char), MAX_LINE, fp) == 0)
    {
        perror("fread");
        thread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return 0;
    }
    if (fseek(fp, 3L, SEEK_SET) == -1)
    {
        perror("fseek");
        thread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return 0;
    }
    if (fread(buf2, sizeof(char), MAX_LINE, fp) == 0)
    {
        perror("fread");
        thread_mutex_unlock(&configAccess);
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
            S1 = convert(tok);
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
            S2 = convert(tok);
        }
        else
            tok = strtok(NULL, " ");
    }
    free(buf2);

    return 1;
}

int openCashier(Cashier_t *ca)
{
    int ret;
    pthread_t thCa;

    pthread_mutex_lock(&ca->accessState);
    ca->open = 1;
    pthread_mutex_unlock(&ca->accessState);

    ret = pthread_create(&thCa, NULL, CashierP, ca);
    if (ret != 0)
    {
        perror("pthread_create");
        return -1;
    }
    if (pthread_detach(thCa) != 0)
    {
        perror("pthread_detach");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int sfd, csfd, optval, sigrecv = 0, ret, pid;
    char msg [2];
    sigset_t set;
    socklen_t optlen = sizeof(optval);
    struct timespec wait_signal;

    unsigned int K, C, E, T, P, S;
    unsigned int S1, S2, countS1, countS2;
    bool found = false;
    Cashier_t *closed_cashier = NULL;

    if (argc != 7)
    {
        fprintf(stdout, "Usage: %s <K> <C> <E> <T> <P> <S>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    K = convert(argv[1]); // max. number of cashiers
    C = convert(argv[2]); // max. number of customers inside supermarket
    E = convert(argv[3]); // number of customers to enter supermarket simultaneously
    T = convert(argv[4]); // max. time to buy products
    P = convert(argv[5]); // max. number of products
    S = convert(argv[6]); // time after customer looks for other cashiers

    if (K <= 0)
    {
        fprintf(stderr, "K should be greater than 0\n");
        exit(EXIT_FAILURE);
    }
    if (C <= 0)
    {
        fprintf(stderr, "C should be greater than 0\n");
        exit(EXIT_FAILURE);
    }
    if (!(E > 0 && E < C))
    {
        fprintf(stderr, "E should be greater than 0 and lower than C \n");
        exit(EXIT_FAILURE);
    }
    if (T <= 0)
    {
        fprintf(stderr, "T should be greater than 0\n");
        exit(EXIT_FAILURE);
    }
    if (S <= 0)
    {
        fprintf(stderr, "S should be greater than 0\n");
        exit(EXIT_FAILURE);
    }
   
    ret = parseS(&S1, &S2);
    if (ret == 0)
    {
        perror("parseS");
        goto error;
    }

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

    // Initialize variables
    if (pthread_mutex_init(&gateCustomers, NULL) != 0)
    {
        perror("pthread_mutex_init");
        goto error;
    }
    if (pthread_cond_init(&exitCustomers, NULL) != 0)
    {
        perror("pthread_cond_init");
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
            perror("execve");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }

    // Wait for a signal (BLOCKING) for 150 ms, repeatedly
    wait_signal.tv_sec = 0;
    wait_signal.tv_nsec = 150 * 1000000; // 150 ms
    countS1 = 0;
    countS2 = 0;
    while (sigrecv != SIGHUP || sigrecv != SIGQUIT)
    {
        if ((sigrecv = sigtimedwait(&set, NULL, 
                                        &wait_signal)) <= 0)
        {
            /* After 150ms if no signal arrived, check
                cashiers' situation */
            if (sigrecv == EAGAIN)
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
                        continue;
                    pthread_mutex_unlock(&cashiers[i]->accessState);

                    pthread_mutex_lock(&cashiers[i]->accessQueue);
                    if (&cashiers[i]->queueCustomers->qlen <= 1)
                        countS1 += 1;
                    if (&cashiers[i]->queueCustomers->qlen >= S2)
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
                perror("sigtimedwait");
                goto error;
            }
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
    kill(pid, SIGKILL);

    close(sfd);
}