#define _POSIX_C_SOURCE 199309L

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include "glob.h"
#include "cashier.h"
#include "customer.h"
#include "director.h"

void free_cu(Customer_t* cu)
{
    if (&cu->finishedTurn)
        pthread_cond_destroy(&cu->finishedTurn);
    if (&cu->startTurn)
        pthread_cond_destroy(&cu->startTurn);
    if (&cu->mutexC)
        pthread_mutex_destroy(&cu->mutexC);

    free(cu);
}

struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;

    if ((end.tv_nsec - start.tv_nsec) < 0)
    {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    }
    else
    {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
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
        if (cashiers[i].queueCustomers != NULL)
        {
            if (cashiers[i].queueCustomers->qlen < c->queueCustomers->qlen)
            {
                c = &(cashiers[i]);
            }
        }
    }
    pthread_mutex_lock(&c->accessQueue);

    if (c != pastCa)
    {
        pastCa = NULL;
        return 1;
    }
    
    pastCa = NULL;
    return 0;
}

void* CustomerP()
{
    struct timespec t, ts_start, ts_end, ts_queue, ts_checkline;
    FILE* fp;
    char* logMsg;               // information for log file
    Cashier_t* ca;              // current cashier
    bool skipCashier = false;   // flag: try another cashier

    unsigned int ret;
    unsigned int nQueue = 0, timeInside, timeQueue;
    unsigned int timeToBuy = rand() % (T - 10 + 1) + 10;
    unsigned int nProd = rand() % (P - 0 + 1);

    Customer_t* cu = (Customer_t*) malloc(sizeof(Customer_t));
    if (cu == NULL)
    {
        perror("malloc");
        goto error;
    }
    if (pthread_cond_init(&cu->finishedTurn, NULL) != 0)
    {
        perror("pthread_cond_init");
        goto error;
    }
    if (pthread_cond_init(&cu->startTurn, NULL) != 0)
    {
        perror("pthread_cond_init");
        goto error;
    }
    if (pthread_mutex_init(&cu->mutexC, NULL) != 0)
    {
        perror("pthread_mutex_init");
        goto error;
    }
    cu->productProcessed = false;
    cu->yourTurn = false;


    // Time spent inside the supermarket
    t.tv_nsec = (timeToBuy % 1000) * 1000000;
    t.tv_sec = timeToBuy / 1000;
    nanosleep(&t, NULL);

    // Did you have zero product? Notify director
    if (nProd == 0)
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

        if (ret == 1) 
        {
            nQueue += 1;
            pthread_mutex_lock(&ca->accessQueue);
            if (ca->queueCustomers == NULL)
            {
                skipCashier = true;
            }

            if (push(ca->queueCustomers, cu) == -1)
            {
                perror("push");
                skipCashier = true;
            }
            pthread_mutex_unlock(&ca->accessQueue);
        }
        
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
    timeQueue = ts_queue.tv_sec / 1000 + ts_queue.tv_nsec * 100000; // to ms
    timeInside = timeToBuy + timeQueue;

    logMsg = (char*) malloc((10*4+3+1)*sizeof(char));
    if (logMsg == NULL)
    {
        perror("malloc");
        goto error;
    }
    sprintf(logMsg, "%10u %10u %10u %10u", timeInside, timeQueue, nQueue, nProd);

    pthread_mutex_lock(&logAccess);
    fp = fopen("../log.txt", "a");
    if (fp == NULL)
    {
        perror("fopen");
        goto error;
    }
    fwrite(logMsg, sizeof(char), len(logMsg), fp);
    fclose(fp);
    pthread_mutex_unlock(&logAccess);

error:
    ca = NULL;
    if (cu != NULL)
        free_cu(cu);
    if (logMsg != NULL)
        free(logMsg);
}