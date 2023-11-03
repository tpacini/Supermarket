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
#include "lib/logger.h"

/* Write customer's statistics on log file.
    Return 0 on success, -1 otherwise. */
static int writeLogCustomer(unsigned int nQueue, unsigned int nProd, 
                     unsigned int timeToBuy, unsigned int timeQueue)
{
    char *logMsg;
    FILE *fp;
    unsigned int timeInside = timeToBuy + timeQueue;

    logMsg = (char *) malloc((10 * 4 + 3 + 1) * sizeof(char));
    if (logMsg == NULL)
    {
        MOD_PERROR("malloc");
        return -1;
    }
    sprintf(logMsg, "%10u %10u %10u %10u", timeInside, timeQueue, 
                                nQueue, nProd);

    pthread_mutex_lock(&logAccess);
    fp = fopen(LOG_FILENAME, "a");
    if (fp == NULL)
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
    if the choosen cashier is equal to the current one (c), the function returns 0, 1 otherwise.

    Assumption: at least one register is open, take the risk of picking a line that could be closed in the meantime. */
static unsigned int chooseCashier (Cashier_t* currentCa)
{
    int i;
    Cashier_t* pastCa = currentCa;

    pthread_mutex_lock(&currentCa->accessQueue);
    /* If no current cashier, pick the first one, start checking
     from the next one */
    if (currentCa == NULL || currentCa->queueCustomers == NULL)
    {
        currentCa = cashiers[0];
        i = 1;
    }
    else
    {
        i = 0;
    }
        
    for (; i < K; i++)
    {
        // No mutex: tradeoff performance-optimal result
        if (cashiers[i]->queueCustomers != NULL)
        {
            if (cashiers[i]->queueCustomers->qlen < \
                currentCa->queueCustomers->qlen)
            {
                pthread_mutex_unlock(&currentCa->accessQueue);
                currentCa = cashiers[i];
                pthread_mutex_lock(&currentCa->accessQueue);
            }
        }
    }
    pthread_mutex_unlock(&currentCa->accessQueue);

    if (currentCa != pastCa)
        return 1;
    
    return 0;
}

int init_customer(Customer_t *cu)
{
    if (cu == NULL)
    {
        cu = (Customer_t *) malloc(sizeof(Customer_t));
        if (cu == NULL)
        {
            MOD_PERROR("malloc");
            return -1;
        }
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
    if (cu == NULL)
        return 0;

    if (pthread_mutex_destroy(&cu->mutexC) != 0 ||
        pthread_mutex_destroy(&cu->accessState) != 0)
    {
        MOD_PERROR("pthread_mutex_destroy");
        free(cu);
        return -1;
    }

    if (pthread_cond_destroy(&cu->finishedTurn) != 0 ||
        pthread_cond_destroy(&cu->startTurn) != 0)
    {
        MOD_PERROR("pthread_cond_destroy");
        free(cu);
        return -1;
    }

    free(cu);
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
    Cashier_t *ca = NULL;               // current cashier data structure
    unsigned int nQueue = 0;            // number of queues visited
    unsigned int timeToBuy;             // time to buy products
    unsigned int ret;                   // return value
    bool skipCashier = false;           // flag: try another cashier

    timeToBuy = rand() % (T - 10 + 1) + 10;

    ts_queue.tv_nsec = 0;
    ts_queue.tv_sec = 0;

    t.tv_nsec = (timeToBuy % 1000) * 1000000;
    t.tv_sec = timeToBuy / 1000;
    nanosleep(&t, NULL);

    // Did you have zero product? Notify director
    if (cu->nProd == 0)
    {
        // TODO: waiting for implementation director's side
        // pthread_mutex_lock(&gateCustomers);
        // while (gateClosed)
        //    pthread_cond_wait(&exitCustomers, &gateCustomers);
        //
        // gateClosed = true;
        // pthread_mutex_unlock(&gateCustomers);
    }
    else
    {
        // Choose cashier. Periodically check for a queue with less customers
        clock_gettime(CLOCK_MONOTONIC, &ts_start);
        while (true) 
        {
            ret = chooseCashier(ca);

            // Line picked or changed
            if (ret == 1) 
            {
                nQueue += 1;
                pthread_mutex_lock(&ca->accessQueue);
                if (ca->queueCustomers == NULL ||
                        push(ca->queueCustomers, cu) == -1)
                {
                    LOG_ERROR("The cashier has been closed");
                    pthread_mutex_unlock(&ca->accessQueue);
                    nQueue -= 1;
                    continue;
                }
                pthread_mutex_unlock(&ca->accessQueue);
            }
                
            // Wait your turn and after some time check new line
            pthread_mutex_lock(&cu->mutexC); 
            clock_gettime(CLOCK_MONOTONIC, &ts_checkqueue);
            ts_checkqueue.tv_nsec += S;

            ret = 0;
            while(!cu->yourTurn && ret != ETIMEDOUT)
                pthread_cond_timedwait(&cu->startTurn, &cu->mutexC, &ts_checkqueue);

            if (cu->yourTurn)
            {
                cu->yourTurn = false;
            }
            // It is not your turn yet, you'll check new lines
            else if (ret == ETIMEDOUT)
            {
                pthread_mutex_unlock(&cu->mutexC);
                continue;
            }
            pthread_mutex_unlock(&cu->mutexC);

            // The cashier is processing your products 
            pthread_mutex_lock(&cu->mutexC);
            while (!cu->productProcessed)
                pthread_cond_wait(&cu->finishedTurn, &cu->mutexC);

            cu->productProcessed = false;
            pthread_mutex_unlock(&cu->mutexC);

            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &ts_end);
        ts_queue = diff(ts_start, ts_end);
    }

    // Write into log file
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

error:
    // Decrease number of customers inside supermarket
    pthread_mutex_lock(&numCu);
    currentNCustomer -= 1;
    pthread_mutex_unlock(&numCu);
    pthread_mutex_lock(&cu->accessState);
    cu->running = 0;
    pthread_mutex_unlock(&cu->accessState);
    
    // Free customer data
    destroy_customer(cu);

    pthread_exit(0);
}