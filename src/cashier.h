#include <stdbool.h>
#include <time.h>
#include "lib/boundedqueue.h"

#pragma once

#define PROD_THRESH 1000
#define NOTIFY_TRESH 5000

typedef struct cashier {
    int id;
    BQueue_t* queueCustomers;
    pthread_mutex_t accessQueue;
    bool open;
    pthread_mutex_t accessState;
    unsigned int totNCustomer;        // number of customers served
    unsigned int totNProds;           // number of processed products
    struct timespec timeOpen;         // time the cashier has been open
    unsigned int nClose;              // number of times closed
    struct timespec meanServiceTime;  // mean service time of customers
    pthread_mutex_t accessLogInfo;
} Cashier_t;

/* Cashier routine executed by a thread */
void CashierP(Cashier_t* ca);

/* 0 on success, -1 otherwise */
int init_cashier(Cashier_t *ca);

/* 0 on success, -1 otherwise */
int destroy_cashier(Cashier_t *ca);

/* Check if the director shutdown the cashier */
bool is_open(Cashier_t* ca);