#define _POSIX_C_SOURCE 199506L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "supermarket.h"
#include "glob.h"
#include "../lib/logger.h"

#define SIGHUP_STR "1"
#define SIGQUIT_STR "3"
#define SIGTERM 15

unsigned int K, C, E, T, P, S;
Customer_t **customers;
Cashier_t **cashiers;
pthread_mutex_t logAccess;
unsigned int currentNCustomer;
unsigned int totNCustomer;
unsigned int totNProd;
pthread_mutex_t numCu;
pthread_mutex_t configAccess;

/* Retrieve number of cashiers to open at startup, from the
    config file. Return number of first cashiers, 0 for errors. */
static unsigned int parseNfc()
{
    FILE *fp;
    char *tok, *buf;
    unsigned int nfc = 0;
    char debug_str[50];

    memset(debug_str, 0, sizeof(debug_str));

    buf = (char *)malloc(MAX_LINE * sizeof(char));
    if (buf == NULL)
    {
        MOD_PERROR("malloc");
        return 0;
    }

    pthread_mutex_lock(&configAccess);
    fp = fopen(CONFIG_FILENAME, "r");
    if (fp == NULL)
    {
        pthread_mutex_unlock(&configAccess);
        MOD_PERROR("fopen");
        free(buf);
        return 0;
    }

    // first line
    if (fgets(buf, MAX_LINE, fp) == NULL)
    {
        pthread_mutex_unlock(&configAccess);
        MOD_PERROR("fgets");
        fclose(fp);
        free(buf);
        return -1;
    }

    memset(buf, 0, MAX_LINE);
    // Read second line
    if (fgets(buf, MAX_LINE, fp) == 0)
    {
        pthread_mutex_unlock(&configAccess);
        MOD_PERROR("fgets");
        fclose(fp);
        free(buf);
        return 0;
    }
    fclose(fp);
    pthread_mutex_unlock(&configAccess);

    tok = strtok(buf, " ");
    while (tok != NULL)
    {
        // Next element is nFirstCashier
        if (strcmp(tok, ":") == 0)
        {
            tok = strtok(NULL, " ");
            errno = 0;
            nfc = strtoul(tok, NULL, 10);
            if (errno == EINVAL || errno == ERANGE)
            {
                MOD_PERROR("strtoul");
                free(buf);
                return 0;
            }
            if (nfc > K)
            {
                MOD_PERROR("0 < nfc < K");
                free(buf);
                return 0;
            }

            sprintf(debug_str, "parsed nfc is %d", nfc);
            LOG_DEBUG(debug_str);
            break;
        }
        else
            tok = strtok(NULL, " ");
    }
    free(buf);

    return nfc;
}

/* Write inside the log file the number of products processed, the number
   of customers served, time open, number of time close and mean service time.
   Return 0 if success, -1 for errors. */
static int writeLogSupermarket()
{
    char *logMsg;
    unsigned int to, mst;
    FILE* fp;

    // Unnecessary, all the threads terminated
    pthread_mutex_lock(&logAccess);
    fp = fopen(LOG_FILENAME, "a");
    if (fp == NULL)
    {
        pthread_mutex_unlock(&logAccess);
        MOD_PERROR("fopen");
        return -1;
    }

    // Write supermarket data
    logMsg = (char *) malloc((10 * 2 + 2 + 1) * sizeof(char));
    if (logMsg == NULL)
    {
        MOD_PERROR("malloc");
        fclose(fp);
        pthread_mutex_unlock(&logAccess);
        return -1;
    }
    sprintf(logMsg, "%10u %10u\n", totNCustomer, totNProd);
    fwrite(logMsg, sizeof(char), sizeof(logMsg), fp);
    free(logMsg);

    // Write cashier's data
    logMsg = (char *) malloc((10 * 5 + 5 + 1) * sizeof(char));
    if (logMsg == NULL)
    {
        MOD_PERROR("malloc");
        fclose(fp);
        pthread_mutex_unlock(&logAccess);
        return -1;
    }

    // Write cashiers data
    for (int i = 0; i < K; i++)
    {
        memset(logMsg, 0, strlen(logMsg));

        // No need to mutex, all threads terminated
        to = timespec_to_ms(cashiers[i]->timeOpen);
        mst = timespec_to_ms(cashiers[i]->meanServiceTime);

        sprintf(logMsg, "%10u %10u %10u %10u %10u\n", cashiers[i]->totNProds, 
                    cashiers[i]->totNCustomer, to, 
                    cashiers[i]->nClose, mst);
        fwrite(logMsg, sizeof(char), sizeof(logMsg), fp);
    }

    free(logMsg);
    fclose(fp);
    pthread_mutex_unlock(&logAccess);
    LOG_DEBUG("Write last information on log file.");

    return 0;
}

/* Wait until all customers exited.
    Return 0 if success, -1 for errors. */
static int waitCustomerTerm()
{
    unsigned int c = 0;
    struct timespec timeout;

    clock_gettime(CLOCK_MONOTONIC, &timeout);
    timeout.tv_sec = 0;
    timeout.tv_nsec = 100 * 1000000; // 100 ms

    while (true)
    {
        pthread_mutex_lock(&numCu);
        c = currentNCustomer;
        pthread_mutex_unlock(&numCu);
        
        if (c == 0)
            break;
        else
        {
            if (nanosleep(&timeout, NULL) != 0)
            {
                MOD_PERROR("nanosleep");
                return -1;
            }
        }
    }

    return 0;
}

/* Wait until there are no more customers waiting to be served,
    and then push a null customer to make the cashier terminate.
    Return 0 if success, -1 for errors. */
static int waitCashierTerm()
{
    int empty = 0;
    void* nullCu = NULL;
    struct timespec timeout;

    clock_gettime(CLOCK_MONOTONIC, &timeout);
    timeout.tv_sec = 0;
    timeout.tv_nsec = 100 * 1000000; // 100 ms

    /* If empty is true, push a null object for every cashier, 
     * the null object when popped will make the cashiers exit */
    for (int i = 0; i < K; i++)
    {
        LOG_DEBUG("Waiting cashier termination.");
        while (!empty)
        {
            if (getLength(cashiers[i]->queueCustomers) == 0)
            {
                push(cashiers[i]->queueCustomers, nullCu);
                empty = 1;
            }
            else if (nanosleep(&timeout, NULL) != 0)
            {
                MOD_PERROR("nanosleep");
                return -1;
            }
        }

        empty = 0;
    }

    return 0;
}

int init_cashier(Cashier_t *ca)
{
    if (!ca)
    {
        LOG_ERROR("ca is NULL.");
        return -1;
    }

    // Max length = C, all customers lined up here
    ca->queueCustomers = initBQueue(C);
    if (!ca->queueCustomers)
    {
        MOD_PERROR("initBQueue");
        free(ca);
        return -1;
    }

    if (pthread_mutex_init(&ca->accessState, NULL) != 0 ||
        pthread_mutex_init(&ca->accessLogInfo, NULL) != 0)
    {
        MOD_PERROR("pthread_mutex_init");
        return -1;
    }

    ca->nClose = 0;
    ca->totNCustomer = 0;
    ca->totNProds = 0;
    ca->open = false;

    return 0;
}

int destroy_cashier(Cashier_t *ca)
{
    if (ca == NULL)
        return 0;

    deleteBQueue(ca->queueCustomers, NULL);

    if (pthread_mutex_destroy(&ca->accessState) != 0 ||
        pthread_mutex_destroy(&ca->accessLogInfo) != 0)
    {
        MOD_PERROR("pthread_mutex_destroy");
        free(ca);
        return -1;
    }

    free(ca);
    return 0;
}

/* Check the number of customers inside the supermarket and if
    less than C-E, it will start E customer threads.
    Return 0 if success, -1 for errors. */
static int enterCustomers()
{
    int ret, remaining = E, running;
    Customer_t *c = NULL;

    // If sufficient space, start E customer threads
    if (currentNCustomer <= C-E)
    {
        pthread_mutex_lock(&numCu);
        currentNCustomer += E;
        pthread_mutex_unlock(&numCu);

        // Find unused data structure
        for (int j = 0; j < C; j++)
        {
            pthread_t thCu;

            pthread_mutex_lock(&customers[j]->accessState);
            running = customers[j]->running;
            pthread_mutex_unlock(&customers[j]->accessState);

            if(!running)
            {
                c = customers[j];

                // Reset data
                c->productProcessed = false;
                c->yourTurn = false;
                c->nProd = rand() % (P - 0 + 1);
                c->running = 1;
                c->id = thCu;

                ret = pthread_create(&thCu, NULL, (void *)CustomerP, c);
                if (ret != 0)
                {
                    MOD_PERROR("pthread_create");
                    return -1;
                }
                if (pthread_detach(thCu) != 0)
                {
                    MOD_PERROR("pthread_detach");
                    return -1;
                }

                remaining -= 1;
            }

            if (remaining == 0)
                break;
        }

        if (remaining != 0)
            LOG_ERROR("Not all E customer threads have been started.");
            
    }

    return 0;
}

void *SupermarketP(void* a)
{
    /* "select" variables */
    fd_set s;
    struct timeval timeout;

    /* Socket variables*/
    struct sockaddr_un dir_addr;
    int sfd = -1;

    /* General variables */
    char *buf = NULL;
    char** argv = (char**) a;
    int ret;
    unsigned int nFirstCashier = 0; // number of cashier to start at time 0

    currentNCustomer = 0;
    totNCustomer = 0;
    totNProd = 0;

    if (pthread_mutex_init(&numCu, NULL) != 0 ||
        pthread_mutex_init(&logAccess, NULL) != 0)
    {
        MOD_PERROR("pthread_mutex_init");
        goto error;
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
    P = strtoul(argv[5], NULL, 10); // max. number of products
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;
    S = strtoul(argv[6], NULL, 10); // time after customer looks for other cashiers
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;

    if (ret == 0)
    {
        MOD_PERROR("error converting arguments");
        goto error;
    }

    if (K <= 0 || C <= 0 || T <= 0 || S <= 0 || P <= 0)
    {
        LOG_ERROR("Parameters should be greater than 0");
        exit(EXIT_FAILURE);
    }
    if (!(E > 0 && E < C))
    {
        LOG_ERROR("E should be greater than 0 and lower than C");
        exit(EXIT_FAILURE);
    }

    LOG_DEBUG("Command line arguments parsed.");

    // Parse nFirstCashier from config
    nFirstCashier = parseNfc();
    if (nFirstCashier == 0)
    {
        MOD_PERROR("parseNfc");
        goto error;
    }

    pthread_mutex_lock(&numCu);
    currentNCustomer = 0;
    pthread_mutex_unlock(&numCu);

    // Initialize cashiers' data
    cashiers = (Cashier_t**) malloc(K*sizeof(Cashier_t*));
    if (cashiers == NULL)
    {
        MOD_PERROR("malloc");
        goto error;
    }

    for (int i = 0; i < K; i++)
    {
        cashiers[i] = (Cashier_t *) malloc(sizeof(Cashier_t));
        if (cashiers[i] == NULL)
        {
            MOD_PERROR("malloc");
            pthread_exit(0);
        }
        
        if(init_cashier(cashiers[i]) != 0)
        {
            MOD_PERROR("init_cashier");
            goto error;
        }
    }

    LOG_DEBUG("Cashiers initialized.");

    // Start cashiers' threads
    for (int i = 0; i < nFirstCashier; i++)
    {
        pthread_t thCa;
        pthread_mutex_lock(&cashiers[i]->accessState);
        cashiers[i]->open = 1;
        pthread_mutex_unlock(&cashiers[i]->accessState);

        ret = pthread_create(&thCa, NULL, CashierP, cashiers[i]);
        if (ret != 0)
        {
            MOD_PERROR("pthread_create");
            goto error;
        }
        
        if (pthread_detach(thCa) != 0)
        {
            MOD_PERROR("pthread_detach");
            goto error;
        }
    }

    LOG_DEBUG("Cashiers started.");

    // Initialize customers' data
    customers = (Customer_t **)malloc(C * sizeof(Customer_t *));
    if (customers == NULL)
    {
        MOD_PERROR("malloc");
        goto error;
    }

    for (int i = 0; i < C; i++)
    {
        customers[i] = (Customer_t *)malloc(sizeof(Customer_t));
        if (customers[i] == NULL)
        {
            MOD_PERROR("malloc");
            pthread_exit(0);
        }

        if (init_customer(customers[i]) == -1)
        {
            MOD_PERROR("init_customer");
            goto error;
        }
    }

    LOG_DEBUG("Customers initialized.");

    // Start customers' threads (C)
    pthread_mutex_lock(&numCu);
    currentNCustomer = C;
    pthread_mutex_unlock(&numCu);
    for (int i = 0; i < C; i++)
    {
        pthread_t thCu;
        pthread_mutex_lock(&customers[i]->accessState);
        customers[i]->running = 1;
        customers[i]->id = thCu;
        pthread_mutex_unlock(&customers[i]->accessState);

        ret = pthread_create(&thCu, NULL, CustomerP, customers[i]);
        if (ret != 0)
        {
            MOD_PERROR("pthread_create");
            goto error;
        }
        if (pthread_detach(thCu) != 0)
        {
            MOD_PERROR("pthread_detach");
            goto error;
        }
    }

    LOG_DEBUG("Customers started.");

    /* CREATE SOCKET TO COMMUNICATE WITH DIRECTOR */
    sfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sfd == -1)
    {
        MOD_PERROR("socket");
        goto error;
    }

    // Build the address
    memset(&dir_addr, 0, sizeof(dir_addr));
    dir_addr.sun_family = AF_UNIX;
    strncpy(dir_addr.sun_path, SOCKET_FILENAME, sizeof(dir_addr.sun_path) - 1);
    
    LOG_DEBUG(dir_addr.sun_path);

    ret = connect(sfd, (struct sockaddr*)&dir_addr, sizeof(dir_addr));
    if (ret == -1)
    {
        MOD_PERROR("connect");
        goto error;
    }

    LOG_DEBUG("Connected socket");

    buf = (char*) malloc((strlen(SIGHUP_STR)+1)*sizeof(char));
    if (buf == NULL)
    {
        MOD_PERROR("malloc");
        goto error;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 100 * 1000; // 100 ms

    while (true)
    {
        FD_ZERO(&s);
        FD_SET(sfd, &s);

        ret = select(sfd+1, &s, NULL, NULL, &timeout);
        if (ret == -1)
        {
            MOD_PERROR("select");
            goto error;
        }
        // timeout expired
        else if (ret == 0)
        {
            ret = enterCustomers();
            if (ret == -1)
            {
                MOD_PERROR("enterCustomers");
                goto error;
            }
        }
        // read ready
        else
        {
            LOG_DEBUG("Message received.");

            if (read(sfd, buf, strlen(SIGHUP_STR)) < 0)
            {
                MOD_PERROR("read");
                goto error;
            }
            buf[strlen(SIGHUP_STR)] = '\0';

            if (strcmp(buf, SIGHUP_STR) != 0 && strcmp(buf, SIGQUIT_STR) != 0)
            {
                LOG_DEBUG("The message is not the one we are looking for.");
                FD_SET(sfd, &s);
                continue;
            }

            // Terminate customers' threads
            if (strcmp(buf, SIGQUIT_STR) == 0) // FIXME: this state is not correct, can cause several issues. Solution: let the customer handle the SIGTERM???
            {
                LOG_DEBUG("Read SIGQUIT signal.");

                for (int i = 0; i < C; i++)
                {
                    ret = pthread_kill(customers[i]->id, SIGTERM); // FIXME: if the customers have locked mutex??? if SIGQUIT
                                                                   //        doesn't destroy mutex???
                    if (ret != 0)
                    {
                        MOD_PERROR("pthread_kill");
                        goto error;
                    }
                    destroy_customer(customers[i]);
                }

                free(customers);
            }
            else // wait customer termination
            {
                LOG_DEBUG("Read SIGHUP signal.");
                
                ret = waitCustomerTerm();
                if (ret != 0)
                {
                    MOD_PERROR("waitCustomerTerm");
                    goto error;
                }
            }

            break;
        }
    }
    free(buf);

    ret = waitCashierTerm();
    if (ret != 0)
    {
        MOD_PERROR("waitCashierTerm");
        goto error;
    }

    LOG_DEBUG("Customers and Cashiers closed.");
        
    ret = writeLogSupermarket();
    if (ret != 0)
    {
        MOD_PERROR("writeLogSupermarket");
        goto error;
    }

    LOG_DEBUG("Saved information to log.");
    
error:
    if (buf != NULL)
        free(buf);

    if (cashiers != NULL)
    {
        for (int i = 0; i < K; i++)
        {
            destroy_cashier(cashiers[i]); // FIXME: there should be a quick way to kill all the cashiers, like pthread_kill and then destroy
        }

        free(cashiers);
    }

    if (customers != NULL)
    {
        for (int i = 0; i < C; i++)
        {
            ret = pthread_kill(customers[i]->id, SIGTERM);
            if (ret != 0)
                LOG_FATAL("Unable to kill customer.");

            ret = destroy_customer(customers[i]);
            if (ret != 0)
                LOG_FATAL("Unable to destroy customer's data.");
        }

        free(customers);
    }



    pthread_mutex_destroy(&numCu);
    pthread_mutex_destroy(&logAccess);
    pthread_mutex_destroy(&configAccess); // FIXME: if it is locked or used by condwait???

    // TODO: NOTIFY DIRECTOR OF THE ERROR

    // TODO: different seed for every thread which used rand_r
    // TODO: fix log data due to errors in project's requirements

    LOG_DEBUG("Closing Supermarket.");

    close(sfd);
    pthread_exit(0);
}