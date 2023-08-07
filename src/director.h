#include "cashier.h"
#include <pthread.h>
#include <stdbool.h>

#pragma once

/* Variables to handle customers with no products */
pthread_cond_t exitCustomers;
pthread_mutex_t gateCustomers;
bool gateClosed;

void DirectorP();

long to_long(char* to_convert);

/* Parse S1 and S2 from the configuration file.
    Return 1 on success, 0 otherwise */
unsigned int parseS(unsigned int *S1, unsigned int *S2);