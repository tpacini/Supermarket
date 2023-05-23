#include <cashier.h>

typedef struct customer {
    pthread_cond_t finishedTurn;
    pthread_cond_t startTurn;
    pthread_mutex_t mutexC;
    bool productProcessed;
    bool yourTurn;

} Customer_t;

/* Customer routine executed by a thread. */
void* CustomerP();

/* Perform the difference between two timespec elements */
struct timespec diff(struct timespec start, struct timespec end);

/* Loop through all the open cashiers and pick the line with less customers,
 * if the choosen cashier is equal to the given one, the function returns 0,
 * 1 otherwise.
 *
 * Assumption: at least one register is open, take the risk of picking a line  * that could be closed in the meantime */
int chooseCashier(Cashier_t* c);