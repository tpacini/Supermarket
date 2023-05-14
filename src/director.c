#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "glob.h"


pthread_mutex_t gateCustomers = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t exitCustomers  = PTHREAD_COND_INITIALIZER;



long to_long(char* to_convert)
{
    char *endptr;
    long val;

    errno = 0; /* To distinguish success/failure after call */
    val = strtol(to_convert, &endptr, 10);

    /* Check for various possible errors. */

    if (errno != 0)
    {
        perror("strtol");
        exit(EXIT_FAILURE);
    }

    if (endptr == to_convert)
    {
        fprintf(stderr, "No digits were found\n");
        exit(EXIT_FAILURE);
    }

    free(endptr);
    return val;
}



int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stdout, "Usage: %s <K> <C> <E>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    K = to_convert(argv[1]);
    C = to_convert(argv[2]);
    E = to_convert(argv[3]);

    if (!(K > 0))
    {
        fprintf(stderr, "K should be greater than 0\n");
        exit(EXIT_FAILURE);
    }

    if (!(E > 0 && E < C))
    {
        fprintf(stderr, "E should be greater than 0 and lower than C \n");
        exit(EXIT_FAILURE);
    }

    if (!(C > 0))
    {
        fprintf(stderr, "C should be greater than 0\n");
        exit(EXIT_FAILURE);
    }


    nCustomers = (long*) malloc(K*sizeof(long));
    if (nCustomers == NULL)
    {
        perror("malloc");
        exit(errno);
    }

    // Start Director
    
    // set gateClosed


    // Start Supermarket
}