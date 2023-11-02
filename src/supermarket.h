#include "cashier.h"
#include "customer.h"

#ifndef __SUPERMARKET_H
#define __SUPERMARKET_H

/* Mutex to access log file */
extern pthread_mutex_t logAccess;

extern unsigned int currentNCustomer;
extern unsigned int totNCustomer;
extern unsigned int totNProd;
extern pthread_mutex_t numCu;

extern Cashier_t **cashiers; // list of all the cashiers

extern Customer_t **customers; // list of all the customers;


#endif