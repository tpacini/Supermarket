#include <stdbool.h>
#include "lib/boundedqueue.h"

#pragma once

typedef struct cashier {
    /* Check or modify queue */
    BQueue_t* queueCustomers;
    pthread_mutex_t accessQueue;
} Cashier;

Cashier* cashiers; // list of all the cashiers
