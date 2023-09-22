#include "cashier.h"

#ifndef __CUSTOMER_H
#define __CUSTOMER_H

typedef struct customer {
    pthread_t id;
    pthread_cond_t finishedTurn;
    pthread_cond_t startTurn;
    pthread_mutex_t mutexC;
    bool productProcessed;
    bool yourTurn;
    unsigned int nProd;
    int running;
    pthread_mutex_t accessState;
} Customer_t;

/* Customer routine executed by a thread. */
void* CustomerP(void *c);

/* Initialize all customer's data.
    Return 0 on success, -1 otherwise */
int init_customer(Customer_t *cu);

/* Free and delete all the allocated customer's data.
    Return 0 on success, -1 otherwise */
int destroy_customer(Customer_t *cu);

#endif