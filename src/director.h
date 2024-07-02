#include <pthread.h>
#include <stdbool.h>

#ifndef __DIRECTOR_H
#define __DIRECTOR_H

/* Variables to handle customers with no products */
extern pthread_cond_t exitCustomers;
extern pthread_mutex_t gateCustomers;
extern unsigned int nCustomerWaiting;
extern bool gateClosed;

#endif