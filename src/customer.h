#include <cashier.h>


typedef struct customer {
    pthread_cond_t finishedTurn;
    pthread_cond_t startTurn;
    pthread_mutex_t mutexC;
    bool productProcessed;
    bool yourTurn;

} Customer;

/***/
void* CustomerP();

/***/
struct timespec diff(struct timespec start, struct timespec end);

/** Assumption: at least one register is open*/
void chooseCashier(Cashier* c);