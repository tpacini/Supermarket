#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#include "cashier.h"
#include "glob.h"
#include "customer.h"

#define MAX_LINE 50
#define PROD_THRESH 1000
#define NOTIFY_TRESH 5000

bool is_open (Cashier_t* ca)
{
    bool result = false;

    pthread_mutex_lock(&ca->accessState);
    if (ca->open)
        result = true;
    pthread_mutex_unlock(&ca->accessState);
    
    return result;
}

void CashierP(Cashier_t* ca)
{
    unsigned char *buf, *tok;
    unsigned int nProd;
    unsigned int totNProd = 0, totNCust = 0;
    struct timespec ts_pTime, ts_tot;
    struct timespec ts_start, ts_end;
    Customer_t *cu;
    FILE *fp;

    unsigned int procTime = rand() % (80 - 20 + 1) + 20;   // processing time
    unsigned int timeProd = 0;                             // time to process single product
    
    struct timespec ts_sTime;
    ts_sTime.tv_sec = 0;
    ts_sTime.tv_nsec = 0;

    // Set time of opening
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    // Retrieve parameter from configuration file
    pthread_mutex_lock(&configAccess);
    fp = fopen('lib/config.txt', 'r');
    if (fp == NULL)
    {
        perror("fopen");
        pthread_mutex_unlock(&configAccess);
        goto error;
    }

    buf = (char*) malloc(MAX_LINE*sizeof(char));
    if (buf == NULL)
    {
        perror("malloc");
        thread_mutex_unlock(&configAccess);
        goto error;
    }

    if (fseek(fp, 0L, SEEK_SET) == -1)
    {
        perror("fseek");
        thread_mutex_unlock(&configAccess);
        goto error;
    }
    if (fread(buf, sizeof(char), MAX_LINE, fp) == 0)
    {
        perror("fseek");
        thread_mutex_unlock(&configAccess);
        goto error;
    }
    tok = strtok(buf, " ");
    while (tok != NULL)
    {
        // Next element is timeProd
        if (strcmp(tok, ":") == 0)
        {
            tok = strtok(NULL, " ");
            timeProd = convert(tok);
            if (timeProd > PROD_THRESH)
            {
                perror("Time to process single product too large");
                thread_mutex_unlock(&configAccess);
                goto error;
            }
        }
        else
            tok = strtok(NULL, " ");
        
    }

    free(buf);
    fclose(fp);
    pthread_mutex_unlock(&configAccess);

    // Start handling customers
    while(is_open(ca))   
    {
        pthread_mutex_lock(&ca->accessQueue);
        cu = pop(ca->queueCustomers);
        pthread_mutex_unlock(&ca->accessQueue);

        // Wake up customer
        pthread_mutex_lock(&cu->mutexC);
        nProd = cu->nProd;
        cu->yourTurn = true;
        pthread_cond_signal(&cu->startTurn);
        pthread_mutex_unlock(&cu->mutexC);

        // Process products (fixed time of cashier + linear time for product)
        ts_pTime = ms_to_timespec(procTime + nProd * timeProd);
        nanosleep(&ts_pTime, NULL);
        
        // Notify customer
        pthread_mutex_lock(&cu->mutexC);
        cu->productProcessed = true;
        pthread_cond_signal(&cu->finishedTurn);
        pthread_mutex_unlock(&cu->mutexC);

        ts_sTime = add_ts(ts_pTime, ts_sTime);
        totNCust += 1;
        totNProd += nProd;
    }

    // Set closing time
    clock_gettime(CLOCK_MONOTONIC, &ts_end);

    // Retrieve the time cashier has been opened
    ts_tot = diff(ts_start, ts_end);

    // Save information of current service
    pthread_mutex_lock(&ca->accessLogInfo);
    ca->meanServiceTime = mean_ts(add_ts(ts_sTime, ca->meanServiceTime) \
                            , totNCust);
    ca->timeOpen = add_ts(ts_tot, ca->timeOpen);
    ca->nCustomer += totNCust;
    ca->nProds += totNProd;
    ca->nClose += 1;
    pthread_mutex_unlock(&ca->accessLogInfo);

error:
    cu = NULL;

    if (buf != NULL)
        free(buf);
    if (ftell(fp) >= 0)
        fclose(fp);
}