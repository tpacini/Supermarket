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
#include "../lib/logger.h"

pthread_cond_t exitCustomers;
pthread_mutex_t gateCustomers;
unsigned int nCustomerWaiting;
bool gateClosed;

/* Parse S1 and S2 from the configuration file.
    Return 0 on success, -1 otherwise */
static int parseS(unsigned int *S1, unsigned int* S2)
{
    FILE* fp;
    char *buf1, *buf2, *tok;
    char debug_str[50];

    memset(debug_str, 0, sizeof(debug_str));

    buf1 = (char*) malloc(MAX_LINE * sizeof(char));
    buf2 = (char*) malloc(MAX_LINE * sizeof(char));
    if (buf1 == NULL || buf2 == NULL)
    {
        MOD_PERROR("malloc");
        return -1;
    }

    fp = fopen(CONFIG_FILENAME, "r");
    if (fp == NULL)
    {
        MOD_PERROR("fopen");
        free(buf1);
        free(buf2);
        return -1;
    }

    // first line
    if(fgets(buf1, MAX_LINE, fp) == NULL)
    {
        MOD_PERROR("fgets");
        fclose(fp);
        free(buf1);
        free(buf2);
        return -1;
    }
    // second line
    if (fgets(buf1, MAX_LINE, fp) == NULL)
    {
        MOD_PERROR("fgets");
        fclose(fp);
        free(buf1);
        free(buf2);
        return -1;
    }

    memset(buf1, 0, MAX_LINE);
    // Read third line
    if (fgets(buf1, MAX_LINE, fp) == 0)
    {
        MOD_PERROR("fgets");
        fclose(fp);
        free(buf1);
        free(buf2);
        return -1;
    }

    // Read fourth line
    if (fgets(buf2, MAX_LINE, fp) == 0)
    {
        MOD_PERROR("fgets");
        fclose(fp);
        free(buf1);
        free(buf2);
        return -1;
    }

    fclose(fp);

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
            if (errno == EINVAL || errno == ERANGE || S1 < 0)
            {
                MOD_PERROR("strtoul");
                free(buf1);
                free(buf2);
                return -1;
            }

            sprintf(debug_str, "S1 is %u", *S1);
            LOG_DEBUG(debug_str);
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
            if (errno == EINVAL || errno == ERANGE || S2 < 0)
            {
                MOD_PERROR("strtoul");
                free(buf2);
                return -1;
            }

            sprintf(debug_str, "S2 is %u", *S2);
            LOG_DEBUG(debug_str);
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

    if (!ca)
    {
        LOG_ERROR("ca is NULL");
        return -1;
    }

    pthread_mutex_lock(&ca->accessState);
    ca->open = 1;
    pthread_mutex_unlock(&ca->accessState);

    ret = pthread_create(&thCa, NULL, CashierP, ca);
    if (ret != 0)
    {
        MOD_PERROR("pthread_create");
        return -1;
    }
    if (pthread_detach(thCa) != 0)
    {
        MOD_PERROR("pthread_detach");
        return -1;
    }

    return 0;
}

/* Check how many customers are queued in Cashiers' lines. Based
   on parameters S1 and S2, the Director will open or close some cashiers
   Return 0 on success, -1 otherwise */
static int checkCashierSituation(unsigned int S1, unsigned int S2, 
                                    unsigned int K)
{
    int ret;
    bool found = false;
    unsigned int countS1 = 0, countS2 = 0;
    Cashier_t *closed_cashier = NULL;
    
    if (K <= 0 || S1 < 0 || S2 < 0)
    {
        LOG_ERROR("Wrong input parameters.");
        return -1;
    }

    for (int i = 0; i < K; i++)
    {
        pthread_mutex_lock(&cashiers[i]->accessState);

        /* The current cashier is closed and the conditions have
            been met -> open cashier */
        if (!cashiers[i]->open && found)
        {
            pthread_mutex_unlock(&cashiers[i]->accessState);
            ret = openCashier(cashiers[i]);
            return ret;
        }
        /* cashier is closed and the conditions have
            not been satisfied yet -> save cashier for
            when conditions will be satisfied
           or, cashier is open and the conditions have
            not been satisfied yet -> look next cashier */
        else if (!cashiers[i]->open)
        {
            closed_cashier = cashiers[i];
            pthread_mutex_unlock(&cashiers[i]->accessState);
            continue;
        }

        pthread_mutex_unlock(&cashiers[i]->accessState);
        
        if (getLength(cashiers[i]->queueCustomers) <= 1)
            countS1 += 1;
        if (getLength(cashiers[i]->queueCustomers) >= S2)
            countS2 += 1;
        
        // Conditions satisfied, open a cashier
        if (countS1 >= S1 || countS2 >= 1)
        {
            found = true;
            if (closed_cashier)
            {
                LOG_DEBUG("Conditions satisfied, opening cashier.");

                ret = openCashier(closed_cashier);
                return ret;
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
    int sfd = -1, csfd = -1;
    int optval;
    socklen_t optlen = sizeof(optval);
    char msg[2];

    // Signal handling variables
    sigset_t set;
    int sigrecv = 0;

    // Other variables
    int ret;                        // return value
    pthread_t thSu;                 // identifier for supermarket thread
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
        MOD_PERROR("error converting arguments");
        exit(EXIT_FAILURE);
    }

    if (K <= 0 || C <= 0 || T <= 0 || S <= 0)
    {
        MOD_PRINTF("Parameters should be greater than 0");
        exit(EXIT_FAILURE);
    }
    if (!(E > 0 && E < C))
    {
        MOD_PRINTF("E should be greater than 0 and lower than C");
        exit(EXIT_FAILURE);
    }

    LOG_DEBUG("Command line arguments parsed.");
   
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
        MOD_PERROR("pthread_mutex_init");
        goto error;
    }
    if (pthread_mutex_init(&gateCustomers, NULL) != 0)
    {
        MOD_PERROR("pthread_mutex_init");
        goto error;
    }
    if (pthread_cond_init(&exitCustomers, NULL) != 0)
    {
        MOD_PERROR("pthread_cond_init");
        goto error;
    }

    nCustomerWaiting = 0;
    pthread_mutex_lock(&gateCustomers);
    gateClosed = true;
    pthread_mutex_unlock(&gateCustomers);

    // Unlink socket file if already present
    ret = remove(SOCKET_FILENAME);
    if (ret == 0) {
        LOG_DEBUG("Socket file removed");
    } else {
        LOG_DEBUG("Socket file does not exists");
    }   

    /* CREATE SOCKET TO COMMUNICATE WITH SUPERMARKET PROCESS */
    sfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sfd == -1)
    {
        MOD_PERROR("socket");
        goto error;
    }

    // Build the address
    memset(&name, 0, sizeof(name));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, SOCKET_FILENAME, sizeof(name.sun_path) - 1);

    LOG_DEBUG(name.sun_path);

    // Keep alive to ensure no unlimited waiting on write/read
    optval = 1; // enable option
    if (setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
    {
        MOD_PERROR("setsockopt");
        goto error;
    }

    // Bind socket to socket name
    if (bind(sfd, (const struct sockaddr*)&name, sizeof(name)) == -1)
    {
        MOD_PERROR("bind");
        goto error;
    }

    LOG_DEBUG("Ready to accept connections.");

    // Prepare for accepting connection
    if (listen(sfd, 1) == -1)
    {
        MOD_PERROR("listen");
        goto error;
    }

    // EXECUTE SUPERMARKET
    ret = pthread_create(&thSu, NULL, SupermarketP, argv);
    if (ret != 0)
    {
        MOD_PERROR("pthread_create");
        goto error;
    }

    if (pthread_detach(thSu) != 0)
    {
        MOD_PERROR("pthread_detach");
        goto error;
    }

    LOG_DEBUG("Supermarket thread created.");

    // Wait for connection
    csfd = accept(sfd, NULL, NULL);
    if (csfd == -1)
    {
        MOD_PERROR("accept");
        goto error;
    }

    LOG_DEBUG("New connection arrived!");

    /* Wait for a signal (BLOCKING) for 150 ms, repeatedly. If the timer runs out, 
     * execute the director routine (look for parameters S1 and S2), otherwise, 
     * if a signal is received, instruct the supermarket process to stop.  
     * */
    wait_signal.tv_sec = 0;
    wait_signal.tv_nsec = 150 * 1000000; // 150 ms
    while (sigrecv != SIGHUP && sigrecv != SIGQUIT)
    {
        errno = 0;
        if ((sigrecv = sigtimedwait(&set, 
                                    NULL, 
                                    &wait_signal)) <= 0)
        {
            // Timeout, no signal arrived
            if (sigrecv == -1 && errno == EAGAIN && cashiers != NULL)
            {
                ret = checkCashierSituation(S1, S2, K);
                if (ret != 0)
                    goto error;

                // Release waiting customers
                pthread_mutex_lock(&gateCustomers);
                pthread_cond_broadcast(&exitCustomers);
                pthread_mutex_unlock(&gateCustomers);
            }
            else if (cashiers == NULL)
            {
                LOG_DEBUG("Cashiers have not been allocated yet.");
                MOD_PRINTF("Waiting for cashiers allocation...");
            }
            else
            {
                MOD_PERROR("sigtimedwait");
                goto error;
            }
        }
    }

    if (sigrecv == SIGHUP)
    {
        LOG_DEBUG("Received signal SIGHUP");
    }
    else
    {
        LOG_DEBUG("Received signal SIGQUIT");
    }

    // Send signal received to supermarket
    sprintf(msg, "%d", sigrecv);
    if (write(csfd, msg, sizeof(msg)) == -1)
    {
        MOD_PERROR("write");
        goto error;
    }

    LOG_DEBUG("Signal sent to supermarket");

    pthread_mutex_lock(&gateCustomers);
    pthread_cond_broadcast(&exitCustomers);
    pthread_mutex_unlock(&gateCustomers);

    // Wait that the supermarket closes
    while(true)
    {
        // Error: the socket is closed, supermarket has been shut down
        ret = write(csfd, msg, sizeof(msg));
        if (ret == -1 && errno == EPIPE)
        {
            LOG_DEBUG("Supermarket closed!\n");
            break;
        }
        else if (ret == -1)
        {
            MOD_PERROR("write");
            goto error;
        }
        else
            sleep(1);
    }

    pthread_mutex_destroy(&gateCustomers);
    pthread_cond_destroy(&exitCustomers);

    remove(SOCKET_FILENAME);
    close(csfd);
    close(sfd);
    return 0;

error:

    if (csfd != -1)
    {
        // Close supermarket, sending a SIGQUIT to it
        LOG_DEBUG("Sending SIQUIT to Supermarket due to error.");
        sprintf(msg, "%d", SIGQUIT);
        if (write(csfd, msg, sizeof(msg)) == -1)
        {
            MOD_PERROR("write");
        }
        else
        {
            // Wait that the supermarket closes
            while (true)
            {
                ret = write(csfd, msg, strlen(msg));
                if (ret == -1 && errno == EPIPE)
                {
                    MOD_PRINTF("Supermarket closed!");
                    break;
                }
                else if (ret == -1)
                {
                    MOD_PERROR("write");
                    break;
                }
                else
                    sleep(1);
            }
        }
        close(csfd);
    }
    
    pthread_mutex_destroy(&gateCustomers);
    pthread_cond_destroy(&exitCustomers);
    remove(SOCKET_FILENAME);

    LOG_DEBUG("Director is exiting...");

    if (sfd != -1)
        close(sfd);
    
    return -1;
}