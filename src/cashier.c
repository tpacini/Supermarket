#include <pthread.h>
#include <errno.h>
#include "cashier.h"



void CashierP()
{
    Cashier_t* ca = (Cashier_t*) malloc(sizeof(Cashier_t));
    if (ca == NULL)
    {
        perror("malloc");
        exit(errno);
    }
    pthread_mutex_init(&ca->accessQueue, NULL);
    ca->queueCustomers = (BQueue_t*) malloc(sizeof(BQueue_t));
    
    //ca->queueCustomers->buf = ....;
    // TODO: Continue malloc of queue
    // TODO: Implement cashier
}