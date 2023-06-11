#include "cashier.h"

#pragma once


unsigned int K, C, E, T, P, S;

Cashier_t **cashiers; // list of all the cashiers

Customer_t **customers; // list of all the customers;





int enterCustomers();

int waitCashierTerm();

int waitCustomerTerm();

int writeLogSupermarket();