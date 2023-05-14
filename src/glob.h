#pragma once

#include <pthread.h>


/* Director's variables */
pthread_mutex_t updateCNumber;
long* nCustomers;                // N. of customers per cashier


/* Director - Customers */
pthread_cond_t exitCustomers;
pthread_mutex_t gateCustomers;
bool gateClosed;

/* aaa */
long K, C, E, T, P;

pthread_mutex_t logAccess;