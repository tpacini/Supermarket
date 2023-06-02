#include <cashier.h>

typedef struct customer {
    pthread_cond_t finishedTurn;
    pthread_cond_t startTurn;
    pthread_mutex_t mutexC;
    bool productProcessed;
    bool yourTurn;
    unsigned int nProd;
} Customer_t;

/* Customer routine executed by a thread. */
void* CustomerP();

/* Free all the object inside the customer's data structure */
void free_cu(Customer_t* cu);

/* Loop through all the open cashiers and pick the line with less customers,
 * if the choosen cashier is equal to the given one, the function returns 0,
 * 1 otherwise.
 *
 * Assumption: at least one register is open, take the risk of picking a line  * that could be closed in the meantime */
int chooseCashier(Cashier_t* c);