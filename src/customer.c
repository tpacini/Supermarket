#define _POSIX_C_SOURCE 199309L

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "customer.h"
#include "glob.h"
#include "supermarket.h"
#include "director.h"
#include "../lib/logger.h"

/* Write customer's statistics on log file.
    Return 0 on success, -1 otherwise. */
static int writeLogCustomer(unsigned int nQueue, unsigned int nProd, 
                     unsigned int timeToBuy, unsigned int timeQueue)
{
    char *logMsg;
    FILE *fp;
    unsigned int timeInside = timeToBuy + timeQueue;

    logMsg = (char *) malloc((10 * 4 + 3 + 1) * sizeof(char));
    if (!logMsg)
    {
        MOD_PERROR("malloc");
        return -1;
    }
    sprintf(logMsg, "%10u %10u %10u %10u", timeInside, timeQueue, 
                                nQueue, nProd);

    pthread_mutex_lock(&logAccess);
    fp = fopen(LOG_FILENAME, "a");
    if (!fp)
    {
        pthread_mutex_unlock(&logAccess);
        MOD_PERROR("fopen");
        free(logMsg);
        return -1;
    }

    fwrite(logMsg, sizeof(char), strlen(logMsg), fp);
    fclose(fp);
    pthread_mutex_unlock(&logAccess);

    return 0;
}

/* Loop through all the open cashiers and pick the line with less customers,
    if new cashier has been selected returns 1, 2 for errors, 0 otherwise. */
static Cashier_t* chooseCashier (Cashier_t* currentCa)
{
    bool open;
    unsigned int start = 0;
    int len1, len2;

    if (!currentCa) 
    {
        // Pick first open cashier
        for (unsigned int i = 0; i < K; i++)
        {
            pthread_mutex_lock(&cashiers[i]->accessState);
            open = cashiers[i]->open;
            pthread_mutex_unlock(&cashiers[i]->accessState);

            if (open)
            {
                currentCa = cashiers[i];
                start = i;
                break;
            }
            
        }
    }

    // No cashier open
    if (!currentCa)
        return NULL;

    for (unsigned int i = start; i < K; i++)
    {
        pthread_mutex_lock(&cashiers[i]->accessState);
        open = cashiers[i]->open;
        pthread_mutex_unlock(&cashiers[i]->accessState);
        if (open)
        {
            len1 = getLength(cashiers[i]->queueCustomers);
            len2 = getLength(currentCa->queueCustomers);
            if (len1 != -1 && len2 != -1 && len1 < len2)
                currentCa = cashiers[i];
        }
    }

    return currentCa;
}

int init_customer(Customer_t *cu)
{
    if (!cu)
    {
        LOG_ERROR("cu is NULL.");
        return -1;
    }

    if (pthread_mutex_init(&cu->mutexC, NULL) != 0 ||
        pthread_mutex_init(&cu->accessState, NULL) != 0)
    {
        MOD_PERROR("pthread_mutex_init");
        return -1;
    }

    if (pthread_cond_init(&cu->finishedTurn, NULL) != 0 ||
        pthread_cond_init(&cu->startTurn, NULL) != 0)
    {
        MOD_PERROR("pthread_cond_init");
        return -1;
    }

    cu->productProcessed = false;
    cu->yourTurn = false;
    cu->nProd = rand() % (P - 0 + 1);
    cu->running = 0;

    return 0;
}

int destroy_customer(Customer_t *cu)
{
    unsigned int error = 0;

    if (!cu)
        return 0;

    if (pthread_mutex_trylock(&cu->mutexC) == 0)
    {
        pthread_mutex_unlock(&cu->mutexC);
        if (pthread_mutex_destroy(&cu->mutexC) != 0)
        {
            MOD_PERROR("pthread_mutex_destroy");
            error = 1;
        }
    }
    else
    {
        MOD_PERROR("pthread_mutex_trylock");
        error = 1;
    }

    if (pthread_mutex_trylock(&cu->accessState) == 0)
    {
        pthread_mutex_unlock(&cu->accessState);
        if (pthread_mutex_destroy(&cu->accessState) != 0)
        {
            MOD_PERROR("pthread_mutex_destroy");
            error = 1;
        }
    }
    else
    {
        MOD_PERROR("pthread_mutex_trylock");
        error = 1;
    }

    if (pthread_cond_destroy(&cu->finishedTurn) != 0 ||
        pthread_cond_destroy(&cu->startTurn) != 0)
    {
        MOD_PERROR("pthread_cond_destroy");
        error = 1;
    }

    free(cu);

    if (error)
        return -1;

    return 0;
}

void* CustomerP(void *c)
{
    /* Timer variables */
    struct timespec ts_start, ts_end, ts_queue; // time spent in the queue(s)
    struct timespec t;              // time spent inside the supermarket
    struct timespec ts_checkqueue;  // time after which you check a new queue

    /* General variables */
    Customer_t *cu = (Customer_t *)c;   // current customer data structure
    Cashier_t *new_ca = NULL;
    Cashier_t *ca = NULL;               // current cashier data structure
    unsigned int nQueue = 0;            // number of queues visited
    unsigned int timeToBuy;             // time to buy products
    unsigned int ret;                   // return value
    unsigned int state = 0;

    timeToBuy = rand() % (T - 10 + 1) + 10;

    ts_queue.tv_nsec = 0;
    ts_queue.tv_sec = 0;

    t.tv_nsec = (timeToBuy % 1000) * 1000000;
    t.tv_sec = timeToBuy / 1000;
    nanosleep(&t, NULL);

    if (cu->nProd == 0) // zero product, notify director
    {
        LOG_DEBUG("Customer with zero product.");
        pthread_mutex_lock(&gateCustomers);
        nCustomerWaiting += 1;
        pthread_cond_wait(&exitCustomers, &gateCustomers);
        nCustomerWaiting -= 1;
        pthread_mutex_unlock(&gateCustomers);
    }
    else
    {
        clock_gettime(CLOCK_MONOTONIC, &ts_start);
        while (cu->running)
        {
            switch(state)
            {
                case 0:
                    new_ca = chooseCashier(ca);
                    if (!new_ca)
                    {
                        LOG_FATAL("Couldn't find a first (open) cashier.");
                        goto error;
                    }
                    else if (new_ca != ca)
                    {
                        LOG_DEBUG("Queue picked or changed.");
                        ca = new_ca;
                        nQueue += 1;
                        if (!ca->queueCustomers ||
                            push(ca->queueCustomers, cu) == -1)
                        {
                            LOG_FATAL("The cashier has been closed, unable to push.");
                            goto error;
                        }
                        else
                            state = 1;
                    }
                    else // queue unchanged
                        state = 1;
                    break;
                case 1:
                    // Wait your turn and after some time check new line
                    pthread_mutex_lock(&cu->mutexC);
                    clock_gettime(CLOCK_MONOTONIC, &ts_checkqueue);
                    ts_checkqueue.tv_nsec += S;

                    ret = 0;
                    while (!cu->yourTurn && ret != ETIMEDOUT)
                        ret = pthread_cond_timedwait(&cu->startTurn, &cu->mutexC, &ts_checkqueue);

                    if (cu->yourTurn)
                    {
                        cu->yourTurn = false;
                        state = 2;
                    }
                    else if (ret == ETIMEDOUT) // not your turn yet, check new lines
                    {
                        pthread_mutex_unlock(&cu->mutexC);
                        state = 0;
                    }
                    else
                    {
                        MOD_PERROR("pthread_cond_timedwait");
                        pthread_mutex_unlock(&cu->mutexC);
                        goto error;
                    }
                    pthread_mutex_unlock(&cu->mutexC);
                    break;
                case 2:
                    LOG_DEBUG("Waiting for cashier to finish processing.");
                    pthread_mutex_lock(&cu->mutexC);
                    while (!cu->productProcessed)
                        pthread_cond_wait(&cu->finishedTurn, &cu->mutexC);

                    cu->productProcessed = false;
                    pthread_mutex_unlock(&cu->mutexC);

                    clock_gettime(CLOCK_MONOTONIC, &ts_end);
                    ts_queue = diff(ts_start, ts_end);

                    state = 3;
                    break;
                case 3:
                    LOG_DEBUG("Writing statistics to log.");
                    ret = writeLogCustomer(nQueue, cu->nProd, timeToBuy,
                                           timespec_to_ms(ts_queue));
                    if (ret != 0)
                    {
                        MOD_PERROR("writeLogCustomer");
                        goto error;
                    }

                    pthread_mutex_lock(&numCu);
                    totNCustomer += 1;
                    totNProd += cu->nProd;
                    pthread_mutex_unlock(&numCu);

                    state = 4;
                    break;
                case 4:
                    LOG_DEBUG("Customer routine successfully completed.");
                    goto error;
                default:
                    LOG_FATAL("Wrong state reached.");
                    goto error;
            };
        }
    }

error:
    // Decrease number of customers inside supermarket
    pthread_mutex_lock(&numCu);
    currentNCustomer -= 1;
    pthread_mutex_unlock(&numCu);
    pthread_mutex_lock(&cu->accessState);
    cu->running = 0;
    pthread_mutex_unlock(&cu->accessState);

    pthread_exit(0);
}