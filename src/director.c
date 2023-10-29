#define _POSIX_C_SOURCE 199506L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "director.h"
#include "glob.h"
#include "supermarket.h"
#include "lib/printer.h"

pthread_mutex_t configAccess;
Cashier_t **cashiers;

/* Parse S1 and S2 from the configuration file.
    Return 0 on success, -1 otherwise */
static int parseS(unsigned int *S1, unsigned int* S2)
{
    FILE* fp;
    char *buf1, *buf2, *tok;

    buf1 = (char*) malloc(MAX_LINE * sizeof(char));
    buf2 = (char*) malloc(MAX_LINE * sizeof(char));
    if (buf1 == NULL || buf2 == NULL)
    {
        DIR_PERROR("parseS, malloc");
        return -1;
    }

    pthread_mutex_lock(&configAccess);
    fp = fopen(CONFIG_FILENAME, "r");
    if (fp == NULL)
    {
        DIR_PERROR("parseS, fopen");
        pthread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return -1;
    }

    // Read third line
    if (fseek(fp, 2L, SEEK_SET) == -1)
    {
        DIR_PERROR("parseS, fseek");
        pthread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return -1;
    }
    if (fread(buf1, sizeof(char), MAX_LINE, fp) == 0)
    {
        DIR_PERROR("parseS, fread");
        pthread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return -1;
    }

    // TODO: debug print buf1

    // Read fourth line
    if (fseek(fp, 3L, SEEK_SET) == -1)
    {
        DIR_PERROR("parseS, fseek");
        pthread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return -1;
    }
    if (fread(buf2, sizeof(char), MAX_LINE, fp) == 0)
    {
        DIR_PERROR("parseS, fread");
        pthread_mutex_unlock(&configAccess);
        free(buf1);
        free(buf2);
        return -1;
    }

    // TODO: debug print buf2

    fclose(fp);
    pthread_mutex_unlock(&configAccess);

    // Parse variable S1 from line read
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
                DIR_PERROR("parseS, strtoul");
                free(buf1);
                free(buf2);
                return -1;
            }

            // TODO: debug printf S1
        }
        else
            tok = strtok(NULL, " ");
    }
    free(buf1);

    // Parse variable S2 from line read
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
                DIR_PERROR("parseS, strtoul");
                free(buf2);
                return -1;
            }

            // TODO: debug printf S2
        }
        else
            tok = strtok(NULL, " ");
    }
    free(buf2);

    return 0;
}

/* Start a new Cashier thread. 
    Return 0 on success, -1 otherwise */
static int openCashier(Cashier_t *ca)
{
    pthread_t thCa;
    int ret;

    if (ca == NULL)
    {
        DIR_PERROR("openCashier, ca is NULL");
        return -1;
    }

    pthread_mutex_lock(&ca->accessState);
    ca->open = 1;
    pthread_mutex_unlock(&ca->accessState);

    ret = pthread_create(&thCa, NULL, CashierP, ca);
    if (ret != 0)
    {
        DIR_PERROR("openCashier, pthread_create");
        return -1;
    }
    if (pthread_detach(thCa) != 0)
    {
        DIR_PERROR("openCashier, pthread_detach");
        return -1;
    }

    return 0;
}

/* Check how many customers are queued in Cashiers' lines. Based
   on parameters S1 and S2, the Director will open or close some cashiers
   Return 0 on success, -1 otherwise */
static int checkCashierSituation(unsigned int S1, unsigned int S2)
{
    int ret;
    bool found = false;
    unsigned int countS1 = 0, countS2 = 0;
    Cashier_t *closed_cashier = NULL;
    

    // FIXME: Could be optimized??

    for (int i = 0; i < K; i++)
    {
        pthread_mutex_lock(&cashiers[i]->accessState);

        /* The current cashier is closed and the conditions have
            been met -> open cashier */
        if (!(&cashiers[i]->open) && found)
        {
            ret = openCashier(cashiers[i]);
            if (ret != 0)
            {
                pthread_mutex_unlock(&cashiers[i]->accessState);
                return -1;
            }
        }
        /* cashier is closed and the conditions have
            not been satisfied yet -> save cashier for
            when conditions will be satisfied
           or, cashier is not closed and the conditions have
            not been satisfied yet -> look next cashier */
        else if (!(&cashiers[i]->open) || found)
        {
            if (!(&cashiers[i]->open))
                closed_cashier = cashiers[i];
            
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
                    return -1;
                else
                    return 0;
            }
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    // Command line arguments
    unsigned int K, C, E, T, S;

    // Configuration file variables
    unsigned int S1 = 0, S2 = 0;

    // Socket communication variables
    struct sockaddr_un name;
    int sfd, csfd = -1;
    int optval;
    socklen_t optlen = sizeof(optval);
    char msg[2];

    // Signal handling variables
    sigset_t set;
    int sigrecv = 0;

    // Other variables
    int ret;                        // return value
    int pid;                        // fork pid
    struct timespec wait_signal;    // main loop waiting time


    /* VARIABLE INITIALIZATION AND ARGUMENT PARSING */
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
        DIR_PERROR("error converting arguments");
        exit(EXIT_FAILURE);
    }

    if (K <= 0 || C <= 0 || T <= 0 || S <= 0)
    {
        DIR_PRINTF("Parameters should be greater than 0");
        exit(EXIT_FAILURE);
    }
    if (!(E > 0 && E < C))
    {
        DIR_PRINTF("E should be greater than 0 and lower than C");
        exit(EXIT_FAILURE);
    }
   
    if (parseS(&S1, &S2) != 0)
        exit(EXIT_FAILURE);

    // Setup signal handler 
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    // Initialize variables
    if (pthread_mutex_init(&configAccess, NULL) != 0)
    {
        DIR_PERROR("pthread_mutex_init");
        goto error;
    }
    if (pthread_mutex_init(&gateCustomers, NULL) != 0)
    {
        DIR_PERROR("pthread_mutex_init");
        goto error;
    }
    if (pthread_cond_init(&exitCustomers, NULL) != 0)
    {
        DIR_PERROR("pthread_cond_init");
        goto error;
    }

    pthread_mutex_lock(&gateCustomers);
    gateClosed = true;
    pthread_mutex_unlock(&gateCustomers);

    /* EXECUTE SUPERMARKET */
    pid = fork();
    if (pid == 0)
    {
        if (execve(SUPMRKT_EXEC_PATH, argv, NULL) == -1)
        {
            DIR_PERROR("execve");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }

    /* CREATE SOCKET TO COMMUNICATE WITH SUPERMARKET PROCESS */
    sfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sfd == -1)
    {
        DIR_PERROR("socket");
        goto error;
    }

    // Build the address
    memset(&name, 0, sizeof(name));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, SOCKET_FILENAME, sizeof(name.sun_path) - 1);

    // TODO: debug: print socket name.sun_path

    // Keep alive to ensure no unlimited waiting on write/read
    optval = 1; // enable option
    if (setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) // TODO: check if setsockopt is needed
    {
        DIR_PERROR("setsockopt");
        goto error;
    }

    // Bind socket to socket name
    if (bind(sfd, (const struct sockaddr*)&name, sizeof(name)) == -1)
    {
        DIR_PERROR("bind");
        goto error;
    }

    // Prepare for accepting connection
    if (listen(sfd, 1) == -1)
    {
        DIR_PERROR("listen");
        goto error;
    }

    // Wait for connection
    csfd = accept(sfd, NULL, NULL);
    if (csfd == -1)
    {
        DIR_PERROR("accept");
        goto error;
    }

    // TODO: debug, "New connection arrived! Socket with the client opened."

    /* Wait for a signal (BLOCKING) for 150 ms, repeatedly. If the timer        timeouts, execute the Director's routine (look for S1 and S2 parameters),
    otherwise if a signal arrives, notify the Supermarket process through
    the socket  */
    wait_signal.tv_sec = 0;
    wait_signal.tv_nsec = 150 * 1000000; // 150 ms
    while (sigrecv != SIGHUP || sigrecv != SIGQUIT)
    {
        errno = 0;
        if ((sigrecv = sigtimedwait(&set, 
                                    NULL, 
                                    &wait_signal)) <= 0)
        {
            // Timeout, no signal arrived
            if (sigrecv == -1 && errno == EAGAIN && cashiers != NULL)
            {
                // TODO: debug "Check cashier situation"

                ret = checkCashierSituation(S1, S2);
                if (ret != 0)
                    goto error;
            }
            else
            {
                DIR_PERROR("sigtimedwait");
                goto error;
            }
        }
    }


    // TODO: debug, "Received signal "sigrecv""

    // Send signal received to supermarket
    sprintf(msg, "%d", sigrecv);
    if (write(csfd, msg, sizeof(msg)) == -1)
    {
        DIR_PERROR("write");
        goto error;
    }

    // TODO: debug, "Message with signal sent to supermarket"

    // Wait that the supermarket closes
    while(true)
    {
        // Error returned and error is "receiving socket closed"
        ret = write(csfd, msg, sizeof(msg));
        if (ret == -1 && errno == EPIPE)
        {
            DIR_PRINTF("Supermarket closed!\n");
            break;
        }
        else if (ret == -1)
        {
            DIR_PERROR("write, wait supermarket close");
            goto error;
        }
        else
            sleep(1);
    }

    // TODO: copy the variables freed or destroyed in error, here

    close(csfd);
    close(sfd);
    return 0;

error: 

    if (csfd != -1)
    {
        // Close supermarket, sending a SIGQUIT to it
        sprintf(msg, "%d", SIGQUIT);
        if (write(csfd, msg, sizeof(msg)) == -1)
        {
            DIR_PERROR("write, error branch");
        }
        else
        {
            // Wait that the supermarket closes
            while (true)
            {
                if (write(csfd, msg, strlen(msg)) == -1 && errno == EPIPE)
                {
                    printf("Supermarket closed!\n");
                    break;
                }
                else
                    sleep(1);
            }
        }
    }
    
    pthread_mutex_destroy(&configAccess);
    pthread_mutex_destroy(&gateCustomers);
    pthread_cond_destroy(&exitCustomers);

    // TODO: debug print director is exiting

    close(sfd);
    close(csfd);
    return -1;
}


// GLOBAL TODO
// TODO: Continue checking code and replacing perror and printf
// TODO: Implement logger debug/warn/fatal/...
// TODO: Differentiate between fatal errors and warnings

// Director should handle customers with zero products, how are they handled and where?

// What happens to the memory when you do the fork. The allocated variables will be duplicated
