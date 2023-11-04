#include <pthread.h>
#include <stdbool.h>

#include "cashier.h"

#ifndef __DIRECTOR_H
#define __DIRECTOR_H

/* Variables to handle customers with no products */
pthread_cond_t exitCustomers;
pthread_mutex_t gateCustomers;
unsigned int nCustomerWaiting;
bool gateClosed;

#endif