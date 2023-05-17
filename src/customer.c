#define _POSIX_C_SOURCE 199309L

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include "glob.h"
#include "cashier.h"
#include "customer.h"


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

void chooseCashier (Cashier* c)
{
    int i = 0;

    if (c == NULL)
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
}

void* CustomerP()
{
    struct timespec t, ts_start, ts_end, ts_queue;
    FILE* fp;
    char* logMsg;        // information for log file
    Cashier* ca;         // current cashier
    Customer* cu;             // customer data
    bool skipC = false;  // flag: try another cashier


    unsigned int nQueue, timeInside, timeQueue;
    unsigned int timeToBuy = rand() % (T - 10 + 1) + 10; 
    unsigned int nProd     = rand() % (P - 0 + 1);


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
    while (!skipC) 
    {
        chooseCashier(ca);

        pthread_mutex_lock(&ca->accessQueue);
        if (ca->queueCustomers == NULL) 
        {
            skipC = true;
        }

        if (push(ca->queueCustomers, cu) == -1)
        {
            perror("push");
            skipC = true;
        }
        pthread_mutex_unlock(&ca->accessQueue);

        if (skipC)
        {
            skipC = false;
            continue;
        }
            
        // Wait your turn and after some time check new line
        // TODO: implement check of other lines every S milliseconds
        pthread_mutex_lock(&cu->mutexC); 
        while(!cu->yourTurn)
            pthread_cond_wait(&cu->startTurn, &cu->mutexC);

        cu->yourTurn = false;
        pthread_mutex_unlock(&cu->mutexC);

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

    logMsg = (char*) malloc(sizeof(char));
    sprintf(logMsg, "");

    pthread_mutex_lock(&logAccess);
    fp = fopen("../log.txt", "a");
    if (fp == NULL)
    {
        perror("fopen");
        exit(errno);
    }
    fwrite(logMsg, sizeof(char), len(logMsg), fp);
    fclose(fp);
    pthread_mutex_unlock(&logAccess);

    free(logMsg);


}