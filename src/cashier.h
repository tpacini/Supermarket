#include <stdbool.h>
#include <time.h>
#include "lib/boundedqueue.h"

#pragma once

typedef struct cashier {
    int id;
    BQueue_t* queueCustomers;
    pthread_mutex_t accessQueue;
    bool open;
    pthread_mutex_t accessState;
    unsigned int nCustomer;           // number of customers served
    unsigned int nProds;              // number of processed products
    struct timespec timeOpen;         // time the cashier has been open
    unsigned int nClose;              // number of times closed
    struct timespec meanServiceTime;  // mean service time of customers
    pthread_mutex_t accessLogInfo;
} Cashier_t;


void CashierP(Cashier_t* ca);

bool is_open(Cashier_t* ca);

int cashier_init(Cashier_t* ca);