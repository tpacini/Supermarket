#include "cashier.h"
#include "customer.h"

#pragma once


unsigned int K, C, E, T, P, S;

Cashier_t **cashiers; // list of all the cashiers

Customer_t **customers; // list of all the customers;

/* Retrieve number of cashiers to open at startup, from the 
    config file. Return number of first cashiers, 0 for errors. */
unsigned int parseNfc();

/* Check the number of customers inside the supermarket and if
    less than C-E, it will start E customer threads.
    Return 0 if success, -1 for errors. */
int enterCustomers();

/* Wait until there are no more customers waiting to be served,
    and then push a null customer to make the cashier terminate.
    Return 0 if success, -1 for errors. */
int waitCashierTerm();

/* Wait until all customers exited. 
    Return 0 if success, -1 for errors. */
int waitCustomerTerm();

/* Write inside the log file the number of products processed, the number
   of customers served, time open, number of time close and mean service time.
   Return 0 if success, -1 for errors. */
int writeLogSupermarket();