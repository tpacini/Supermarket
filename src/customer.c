#define _POSIX_C_SOURCE 199309L

#include <time.h>
#include "glob.h"


struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;

    if ((end.tv_nsec - start.tv_nsec) < 0)
    {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    }
    else
    {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}

void* Customer()
{
    struct timespec t, ts_start, ts_end, ts_queue;

    unsigned int nQueue, timeInside, timeQueue;
    unsigned int timeToBuy = rand() % (T - 10 + 1) + 10; 
    unsigned int nProd     = rand() % (P - 0 + 1);


    // Time spent inside the supermarket
    t.tv_nsec = (timeToBuy % 1000) * 1000000;
    t.tv_sec = timeToBuy / 1000;
    nanosleep(&t, NULL);

    // Did you have zero product? Notify director
    if (nProd == 0)
    {
        pthread_mutex_lock(&gateCustomers);
        while (gateClosed)
            pthread_cond_wait(&exitCustomers, &gateCustomers);
        pthread_mutex_unlock(&gateCustomers);
    }

    // Choose cashier, periodically check new line with less customers

        // Check number of open lines and choose one

        // Push yourself in that queue

        // Wait your turn and after some time check new line


    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    ts_queue = diff(ts_start, ts_end);


    // ....


    // Write into log file
    
    timeQueue = ts_queue.tv_sec / 1000 + ts_queue.tv_nsec * 100000; // to ms
    timeInside = timeToBuy + timeQueue;


}