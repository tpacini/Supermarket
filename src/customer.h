#include <cashier.h>

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
void* CustomerP(Customer_t *cu);

/* Loop through all the open cashiers and pick the line with less customers,
    if the choosen cashier is equal to the current one (c), the function returns 0, 1 otherwise.

    Assumption: at least one register is open, take the risk of picking a line that could be closed in the meantime. */
int chooseCashier(Cashier_t* c);

/* Initialize all customer's data.
    Return 0 on success, -1 otherwise */
int init_customer(Customer_t *cu);

/* Free and delete all the allocated customer's data.
    Return 0 on success, -1 otherwise */
int destroy_customer(Customer_t *cu);

/* Write customer's statistics on log file.
    Return 0 on success, -1 otherwise. */
int writeLogCustomer(unsigned int nQueue, unsigned int nProd,
                     unsigned int timeToBuy, unsigned int timeQueue);