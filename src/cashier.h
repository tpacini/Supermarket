#include <stdbool.h>
#include "lib/boundedqueue.h"

#pragma once

typedef struct cashier {
    BQueue_t* queueCustomers;
    pthread_mutex_t accessQueue;
} Cashier_t;


void CashierP();