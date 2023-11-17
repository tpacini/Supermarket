#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include "cashier.h"
#include "customer.h"
#include "glob.h"
#include "../lib/logger.h"

// TODO: check the entire file

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
    // Max length = C, all customers lined up here
    ca->queueCustomers = initBQueue(C);
    if (ca->queueCustomers == NULL)
    {
        MOD_PERROR("initBQueue");
        if (ca != NULL)
            free(ca);
        return -1;
    }

    if (pthread_mutex_init(&ca->accessQueue, NULL) != 0 ||
        pthread_mutex_init(&ca->accessState, NULL) != 0 ||
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

    if (pthread_mutex_destroy(&ca->accessQueue) != 0 ||
        pthread_mutex_destroy(&ca->accessState) != 0 ||
        pthread_mutex_destroy(&ca->accessLogInfo) != 0)
    {
        MOD_PERROR("pthread_mutex_destroy");
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
    char *buf, *tok;
    char debug_str[50];

    buf = (char*) malloc(MAX_LINE * sizeof(char));
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
        // Next element is timeProd
        if (strcmp(tok, ":") == 0)
        {
            tok = strtok(NULL, " ");
            errno = 0;
            timeProd = strtoul(tok, NULL, 10);
            if (errno == EINVAL || errno == ERANGE)
            {
                MOD_PERROR("strtoul");
                free(buf);
                return 0;
            }
            if (timeProd > PROD_THRESH)
            {
                LOG_ERROR("timeProd should not be NULL or too large");
                free(buf);
                return 0;
            }

            sprintf(debug_str, "timeProd is %d", timeProd);
            LOG_DEBUG(debug_str);
        }
        else
            tok = strtok(NULL, " ");
    }
    free(buf);

    return timeProd;
}

void* CashierP(void *c)
{
    /* Log variables */
    struct timespec ts_sTime; // service time
    struct timespec ts_pTime; // time to process products

    /* General variables */
    unsigned int procTime = rand() % (80 - 20 + 1) + 20; // processing time
    unsigned int timeProd = 0;           // time to process single product
    unsigned int nProd;                  // number of products to process
    Customer_t *cu = NULL;               // current customer pointer
    Cashier_t *ca  = (Cashier_t *)c;     // current cashier's data structure
    struct timespec ts_start, ts_end, ts_tot;

    ts_sTime.tv_sec = 0;
    ts_sTime.tv_nsec = 0;

    // Retrieve parameter from configuration file
    timeProd = parseTimeProd();
    if (timeProd == 0)
    {
        MOD_PERROR("parseTimeProd");
        goto error;
    }

    // Set time of opening
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    // Start handling customers
    while(is_open(ca))   
    {
        // Pop a customer from the queue
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
    if (ca->totNCustomer == 0)
    {
        pthread_mutex_unlock(&ca->accessLogInfo);
        MOD_PERROR("division by zero");
        goto error;
    }

    ca->meanServiceTime = add_ts(ca->meanServiceTime, ts_sTime);
    ca->meanServiceTime.tv_nsec = round(ca->meanServiceTime.tv_nsec \
                                            / ca->totNCustomer);
    ca->meanServiceTime.tv_sec = round(ca->meanServiceTime.tv_sec \
                                            / ca->totNCustomer);

    ca->timeOpen = add_ts(ca->timeOpen, ts_tot);
    ca->nClose += 1;
    pthread_mutex_unlock(&ca->accessLogInfo);

error:

    // ca->nClose += 1
    pthread_exit(0);
}