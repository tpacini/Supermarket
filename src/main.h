#include "cashier.h"

#pragma once


unsigned int K, C, E, T, P, S;

Cashier_t** cashiers; // list of all the cashiers

/* 0 on success, -1 otherwise */
int init_cashier(Cashier_t* ca);

/* 0 on success, -1 otherwise */
int destroy_cashier(Cashier_t* ca);

int enterCustomers();

int waitCashierExit();

int writeLogSupermarket();