#define _POSIX_C_SOURCE 199309L

#include <time.h>
#include <stdio.h>
#include <errno.h>

#include "glob.h"
#include "cashier.h"
#include "customer.h"
#include "supermarket.h"
#include "director.h"

int writeLogCustomer(unsigned int nQueue, unsigned int nProd, 
                     unsigned int timeToBuy, unsigned int timeQueue)
{
    char *logMsg;
    FILE *fp;
    unsigned int timeInside = timeToBuy + timeQueue;

    logMsg = (char *)malloc((10 * 4 + 3 + 1) * sizeof(char));
    if (logMsg == NULL)
    {
        perror("malloc");
        return -1;
    }
    sprintf(logMsg, "%10u %10u %10u %10u", timeInside, timeQueue, 
                                nQueue, nProd);

    pthread_mutex_lock(&logAccess);
    fp = fopen(LOG_FILENAME, "a");
    if (fp == NULL)
    {
        perror("fopen");
        pthread_mutex_unlock(&logAccess);
        free(logMsg);
        return -1;
    }
    fwrite(logMsg, sizeof(char), len(logMsg), fp);
    fclose(fp);
    pthread_mutex_unlock(&logAccess);

    return 0;
}

int chooseCashier (Cashier_t* c)
{
    int i = 0;
    Cashier_t* pastCa = c;

    pthread_mutex_lock(&c->accessQueue);
    if (c == NULL || c->queueCustomers == NULL)
    {
        c = &(cashiers[0]);
        i = 1;
    }
        
    for (; i < K; i++)
    {
        // No mutex: tradeoff performance-optimal result
        if (cashiers[i]->queueCustomers != NULL)
        {
            if (cashiers[i]->queueCustomers->qlen < c->queueCustomers->qlen)
            {
                c = &(cashiers[i]);
            }
        }
    }
    pthread_mutex_lock(&c->accessQueue);

    if (c != pastCa)
        return 1;
    
    return 0;
}

int init_customer(Customer_t *cu)
{
    if (cu == NULL)
    {
        cu = (Customer_t *)malloc(sizeof(Customer_t));
        if (cu == NULL)
        {
            perror("malloc");
            return -1;
        }
    }

    if (pthread_mutex_init(&cu->mutexC, NULL) != 0 ||
        pthread_mutex_init(&cu->accessState, NULL) != 0)
    {
        perror("pthread_mutex_init");
        return -1;
    }

    if (pthread_cond_init(&cu->finishedTurn, NULL) != 0 ||
        pthread_cond_init(&cu->startTurn, NULL) != 0)
    {
        perror("pthread_cond_init");
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
        perror("pthread_mutex_destroy");
        return -1;
    }

    if (pthread_cond_destroy(&cu->finishedTurn) != 0 ||
        pthread_cond_destroy(&cu->startTurn) != 0)
    {
        perror("pthread_cond_destroy");
        return -1;
    }

    free(cu);
    return 0;
}

void* CustomerP(Customer_t *cu)
{
    struct timespec t, ts_start, ts_end, ts_queue, ts_checkline;
    Cashier_t* ca;              // current cashier
    bool skipCashier = false;   // flag: try another cashier

    unsigned int ret;
    unsigned int nQueue = 0, timeInside, timeQueue;
    unsigned int timeToBuy = rand() % (T - 10 + 1) + 10;

    // Time spent inside the supermarket
    t.tv_nsec = (timeToBuy % 1000) * 1000000;
    t.tv_sec = timeToBuy / 1000;
    nanosleep(&t, NULL);

    // Did you have zero product? Notify director
    if (cu->nProd == 0)
    {
        pthread_mutex_lock(&gateCustomers);
        while (gateClosed)
            pthread_cond_wait(&exitCustomers, &gateCustomers);
        
        gateClosed = true;
        pthread_mutex_unlock(&gateCustomers);
    }

    // Choose cashier, periodically check for a line with less customers
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    while (!skipCashier) 
    {
        ret = chooseCashier(ca);

        // Line picked or changed
        if (ret == 1) 
        {
            nQueue += 1;
            pthread_mutex_lock(&ca->accessQueue);
            if (ca->queueCustomers == NULL)
            {
                skipCashier = true;
            }

            // Push customer in the queue
            if (push(ca->queueCustomers, cu) == -1)
            {
                perror("push");
                skipCashier = true;
            }
            pthread_mutex_unlock(&ca->accessQueue);
        }
        
        // The cashier has been closed, choose another cashier
        if (skipCashier)
        {
            nQueue -= 1;
            skipCashier = false;
            continue;
        }
            
        // Wait your turn and after some time check new line
        pthread_mutex_lock(&cu->mutexC); 
        clock_gettime(CLOCK_MONOTONIC, &ts_checkline);
        ts_checkline.tv_nsec += S;
        ret = 0;
        while(!cu->yourTurn && ret != ETIMEDOUT)
            pthread_cond_timedwait(&cu->startTurn, &cu->mutexC, &ts_checkline);

        if (cu->yourTurn)
        {
            cu->yourTurn = false;
            ret = 0;
        }
        pthread_mutex_unlock(&cu->mutexC);

        // It is not your turn yet, you'll check new lines
        if (ret == ETIMEDOUT)
            continue;

        // The cashier is processing your products 
        pthread_mutex_lock(&cu->mutexC);
        while (!cu->productProcessed)
            pthread_cond_wait(&cu->finishedTurn, &cu->mutexC);

        cu->productProcessed = false;
        pthread_mutex_unlock(&cu->mutexC);
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    ts_queue = diff(ts_start, ts_end);

    // Write into log file
    ret = writeLogCustomer(nQueue, cu->nProd, timeToBuy, 
                        timespec_to_ms(ts_queue));
    if (ret != 0)
    {
        perror("writeLogCustomer");
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