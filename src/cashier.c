#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#include "cashier.h"
#include "glob.h"
#include "supermarket.h"

/* Check if the cashiers is open (concurrent safe).
    Return true if it is open, false otherwise */
static bool is_open (Cashier_t* ca)
{
    bool result = false;

    pthread_mutex_lock(&ca->accessState);
    if (ca->open)
        result = true;
    pthread_mutex_unlock(&ca->accessState);
    
    return result;
}

int init_cashier(Cashier_t *ca)
{
    if (ca == NULL)
    {
        ca = (Cashier_t *)malloc(sizeof(Cashier_t));
        if (ca == NULL)
        {
            perror("malloc");
            return -1;
        }
    }

    // If all customers inside the supermarket are in this line
    // --> length = C
    if ((ca->queueCustomers = initBQueue(C)) == NULL)
    {
        perror("initBQueue");
        if (ca != NULL)
            free(ca);
        return -1;
    }

    if (pthread_mutex_init(&ca->accessQueue, NULL) != 0 ||
        pthread_mutex_init(&ca->accessState, NULL) != 0 ||
        pthread_mutex_init(&ca->accessLogInfo, NULL) != 0)
    {
        perror("pthread_mutex_init");
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

    if (pthread_mutex_destroy(&ca->accessQueue) != 0 ||
        pthread_mutex_destroy(&ca->accessState) != 0 ||
        pthread_mutex_destroy(&ca->accessLogInfo) != 0)
    {
        perror("pthread_mutex_destroy");
        free(ca);
        return -1;
    }

    free(ca);
    return 0;
}

/* Parse time required to process a single product,
    from the configuration file. Return 0 on errors. */
static unsigned int parseTimeProd()
{
    unsigned int timeProd;
    FILE* fp;
    unsigned char *buf, *tok;

    buf = (char *)malloc(MAX_LINE * sizeof(char));
    if (buf == NULL)
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
        free(buf);
        return 0;
    }

    if (fseek(fp, 0L, SEEK_SET) == -1)
    {
        perror("fseek");
        thread_mutex_unlock(&configAccess);
        free(buf);
        return 0;
    }
    if (fread(buf, sizeof(char), MAX_LINE, fp) == 0)
    {
        perror("fread");
        thread_mutex_unlock(&configAccess);
        free(buf);
        return 0;
    }
    fclose(fp);
    pthread_mutex_unlock(&configAccess);

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
                perror("Time to process single product NULL or \
                        too large");
                free(buf);
                return 0;
            }
        }
        else
            tok = strtok(NULL, " ");
    }
    free(buf);

    return timeProd;
}

void CashierP(Cashier_t *ca)
{
    unsigned int nProd;
    struct timespec ts_pTime, ts_tot;
    struct timespec ts_start, ts_end;
    Customer_t *cu;

    unsigned int procTime = rand() % (80 - 20 + 1) + 20;   // processing time
    unsigned int timeProd = 0;                             // time to process single product
    
    struct timespec ts_sTime;
    ts_sTime.tv_sec = 0;
    ts_sTime.tv_nsec = 0;

    // Retrieve parameter from configuration file
    timeProd = parseTimeProd();
    if (timeProd == 0)
    {
        perror("parseTimeProd");
        goto error;
    }

    // Set time of opening
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    // Start handling customers
    while(is_open(ca))   
    {
        // Pop a customer
        pthread_mutex_lock(&ca->accessQueue);
        cu = pop(ca->queueCustomers);
        pthread_mutex_unlock(&ca->accessQueue);

        // NULL customer, close immediately the cashier
        if (cu == NULL)
        {
            break;
        }

        // Wake up customer
        pthread_mutex_lock(&cu->mutexC);
        nProd = cu->nProd;
        cu->yourTurn = true;
        pthread_cond_signal(&cu->startTurn);
        pthread_mutex_unlock(&cu->mutexC);

        // Process products (fixed time of cashier + linear time for product)
        ts_pTime = ms_to_timespec(procTime + nProd * timeProd);
        nanosleep(&ts_pTime, NULL);
        
        // Notify customer that products have been processed
        pthread_mutex_lock(&cu->mutexC);
        cu->productProcessed = true;
        pthread_cond_signal(&cu->finishedTurn);
        pthread_mutex_unlock(&cu->mutexC);

        // Update statistics
        ts_sTime = add_ts(ts_pTime, ts_sTime);
        ca->totNCustomer += 1;
        ca->totNProds += nProd;
    }

    // Set closing time
    clock_gettime(CLOCK_MONOTONIC, &ts_end);

    // Retrieve the time cashier has been opened
    ts_tot = diff(ts_start, ts_end);

    // Save information of current service
    pthread_mutex_lock(&ca->accessLogInfo);
    ca->meanServiceTime = mean_ts(add_ts(ts_sTime, ca->meanServiceTime) \
                            , ca->totNCustomer);
    ca->timeOpen = add_ts(ts_tot, ca->timeOpen);
    ca->nClose += 1;
    pthread_mutex_unlock(&ca->accessLogInfo);

error:

    pthread_exit(0);
}