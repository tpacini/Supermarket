#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "glob.h"
#include "main.h"
#include "customer.h"

unsigned int parseConfigFile()
{
    FILE *fp;
    char *tok, *buf;
    unsigned int nfc;

    buf = (char *)malloc(MAX_LINE * sizeof(char));
    if (buf == NULL)
    {
        perror("malloc");
        return 0;
    }

    pthread_mutex_lock(&configAccess);
    fp = fopen(CONFIG_FILENAME, 'r');
    if (fp == NULL)
    {
        perror("fopen");
        thread_mutex_unlock(&configAccess);
        free(buf);
        return 0;
    }
    
    if (fseek(fp, 1L, SEEK_SET) == -1)
    {
        perror("fseek");
        thread_mutex_unlock(&configAccess);
        fclose(fp);
        free(buf);
        return 0;
    }

    if (fread(buf, sizeof(char), MAX_LINE, fp) == 0)
    {
        perror("fread");
        thread_mutex_unlock(&configAccess);
        fclose(fp);
        free(buf);
        return 0;
    }
    fclose(fp);

    tok = strtok(buf, " ");
    while (tok != NULL)
    {
        // Next element is nFirstCashier
        if (strcmp(tok, ":") == 0)
        {
            tok = strtok(NULL, " ");
            nfc = convert(tok);
            if (nfc <= 0 || nfc > K)
            {
                perror("Number of initial cashiers should be higher \
                        than 0 and lower than K");
                thread_mutex_unlock(&configAccess);
                free(buf);
                return 0;
            }
        }
        else
            tok = strtok(NULL, " ");
    }

    free(buf);
    thread_mutex_unlock(&configAccess);

    return nfc;
}

int writeLogSupermarket()
{
    char *logMsg;
    unsigned int to, mst;
    FILE* fp;

    // No need to mutex, all the threads terminated
    fp = fopen(LOG_FILENAME, 'a');
    if (fp == NULL)
    {
        perror("fopen");
        return -1;
    }

    // Write supermarket data
    logMsg = (char *)malloc((10 * 2 + 2 + 1) * sizeof(char));
    if (logMsg == NULL)
    {
        perror("malloc");
        close(fp);
        return -1;
    }
    sprintf(logMsg, "%10u %10u\n", totNCustomer, totNProd);
    fwrite(logMsg, sizeof(char), strlen(logMsg), fp);
    free(logMsg);

    // Write cashier's data
    logMsg = (char *)malloc((10 * 4 + 4 + 1) * sizeof(char));
    if (logMsg == NULL)
    {
        perror("malloc");
        close(fp);
        return -1;
    }

    // Write cashiers data
    for (int i = 0; i < K; i++)
    {
        memset(logMsg, 0, strlen(logMsg));

        // No need to mutex, all threads terminated
        to = timespec_to_ms(cashiers[i]->timeOpen);
        mst = timespec_to_ms(cashiers[i]->meanServiceTime);

        sprintf(logMsg, "%10u %10u %10u %10u\n", cashiers[i]->nCustomer,
                to, cashiers[i]->nClose, mst);
        fwrite(logMsg, sizeof(char), strlen(logMsg), fp);
    }

    free(logMsg);
    fclose(fp);

    return 0;
}

int waitCashierExit()
{
    int empty = 0;
    void* nullCu = NULL;
    struct timespec timeout;

    CLOCK_GETTIME(CLOCK_MONOTONIC, &timeout);
    timeout.tv_sec = 0;
    timeout.tv_nsec = 100 * 1000000; // 100 ms

    /* If empty is true, push a null object for every cashier, 
     * the null object when popped will make the cashiers exit */
    for (int i = 0; i < K; i++)
    {
        // Wait that cashier handles customers
        while (!empty)
        {
            pthread_mutex_lock(&cashiers[i]->accessQueue);
            if (cashiers[i]->queueCustomers->qlen == 0)
            {
                push(cashiers[i]->queueCustomers, nullCu);
                empty = 1;
            }
            pthread_mutex_unlock(&cashiers[i]->accessQueue);

            if (!empty)
            {
                if (nanosleep(&timeout, NULL) != 0)
                {
                    perror("nanosleep");
                    return -1;
                }
            }
        }

        empty = 0;
    }

    return 0;
}


int enterCustomer()
{
    int n, ret;

    pthread_mutex_lock(&numCu);
    n = nCustomer;
    pthread_mutex_lock(&numCu);

    // Start E customer threads
    if (nCustomer <= C-E)
    {
        pthread_mutex_lock(&numCu);
        nCustomer += E;
        pthread_mutex_lock(&numCu);
        for (int i = 0; i < E; i++)
        {
            pthread_t thCu;

            ret = pthread_create(&thCu, NULL, CustomerP, NULL);
            if (ret != 0)
            {
                perror("pthread_create");
                return -1;
            }
            if (pthread_detach(thCu) != 0)
            {
                perror("pthread_detach");
                return -1;
            }
        }
    }

    return 0;
}

int init_cashier(Cashier_t *ca)
{
    if (ca == NULL)
    {
        ca = (Cashier_t*) malloc(sizeof(Cashier_t));
        if (ca == NULL)
        {
            perror("malloc");
            return -1;
        }
    }

    // If all customers inside the supermarket are in this line
    // --> length = C
    if ((ca->queueCustomers = initBQueue(C)) == NULL)
    {
        perror("initBQueue");
        return -1;
    }

    if (pthread_mutex_init(&ca->accessQueue, NULL) != 0 ||
        pthread_mutex_init(&ca->accessState, NULL) != 0)
    {
        perror("pthread_mutex_init");
        return -1;
    }

    // open = false

    // TO CONTINUE

    return 0;
}

int destroy_cashier(Cashier_t* ca)
{

    // TO CONTINUE

    free(ca);
    return 0;
}

int main(int argc, char* argv[])
{
    char* buf;
    int sfd, ret;
    unsigned int nFirstCashier;  // number of cashier to start at time 0

    /* Variables for "select" */
    fd_set s;
    struct timeval timeout;

    /* Global variables for log file */
    unsigned int totNCustomer = 0;
    unsigned int totNProd = 0;
    

    if (argc != 7)
    {
        fprintf(stdout, "Usage: %s <K> <C> <E> <T> <P> <S>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    K = convert(argv[1]); // max. number of cashiers
    C = convert(argv[2]); // max. number of customers inside supermarket
    E = convert(argv[3]); // number of customers to enter supermarket simultaneously
    T = convert(argv[4]); // max. time to buy products
    P = convert(argv[5]); // max. number of products
    S = convert(argv[6]); // time after customer looks for other cashiers

    if (K <= 0)
    {
        fprintf(stderr, "K should be greater than 0\n");
        exit(EXIT_FAILURE);
    }
    if (C <= 0)
    {
        fprintf(stderr, "C should be greater than 0\n");
        exit(EXIT_FAILURE);
    }
    if (!(E > 0 && E < C))
    {
        fprintf(stderr, "E should be greater than 0 and lower than C \n");
        exit(EXIT_FAILURE);
    }
    if(T <= 0)
    {
        fprintf(stderr, "T should be greater than 0\n");
        exit(EXIT_FAILURE);
    }
    if (S <= 0)
    {
        fprintf(stderr, "S should be greater than 0\n");
        exit(EXIT_FAILURE);
    }

    // Parse nFirstCashier from config
    nFirstCashier = parseConfigFile();
    if (nFirstCashier == 0)
    {
        perror("parseConfigFile");
        goto error;
    }

    pthread_mutex_lock(&numCu);
    nCustomer = 0;
    pthread_mutex_unlock(&numCu);

    // Initialize cashiers' data
    cashiers = (Cashier_t**) malloc(K*sizeof(Cashier_t*));
    if (cashiers == NULL)
    {
        perror("malloc");
        goto error;
    }
    for (int i = 0; i < K; i++)
    {
        if(init_cashier(cashiers[i]) == -1)
        {
            perror("init_cashier");
            goto error;
        }
    }

    // Start cashiers' threads
    for (int i = 0; i < nFirstCashier; i++)
    {
        pthread_t thCa;
        pthread_mutex_lock(&cashiers[i]->accessState);
        cashiers[i]->open = 1;
        pthread_mutex_unlock(&cashiers[i]->accessState);
        
        ret = pthread_create(&thCa, NULL, CashierP, cashiers[i]);
        if (ret != 0)
        {
            perror("pthread_create");
            goto error;
        }
        if (pthread_detach(thCa) != 0)
        {
            perror("pthread_detach");
            goto error;
        }
    }

    // Start customers' threads (C)
    pthread_mutex_lock(&numCu);
    nCustomer = C;
    pthread_mutex_unlock(&numCu);
    for (int i = 0; i < C; i++)
    {
        pthread_t thCu;

        ret = pthread_create(&thCu, NULL, CustomerP, NULL);
        if (ret != 0)
        {
            perror("pthread_create");
            goto error;
        }
        if (pthread_detach(thCu) != 0)
        {
            perror("pthread_detach");
            goto error;
        }
    }

    // Handling SIGHUP or SIGQUIT from Director
    sfd = socket(AF_UNIX, SOCK_STREAM, 0); // ip protocol
    if (sfd == -1)
    {
        perror("socket");
        goto error;
    }
    ret = connect(sfd, NULL, 0);
    if (ret == -1)
    {
        perror("connect");
        goto error;
    }

    buf = (char*) malloc((1+1)*sizeof(char));
    if (buf == NULL)
    {
        perror("malloc");
        goto error;
    }

    FD_ZERO(&s);
    FD_SET(sfd, &s);

    timeout.tv_sec = 0;
    timeout.tv_usec = 100 * 1000; // 100 ms

    while (true)
    {
        ret = select(sfd, &s, NULL, NULL, &timeout);
        if (ret == -1)
        {
            perror("select");
            goto error;
        }
        // timeout expired
        else if (ret == 0)
        {
            ret = enterCustomers();
            if (ret == -1)
            {
                perror("enterCustomers");
                goto error;
            }
        }
        // read ready
        else
        {
            if (read(sfd, buf, 1) < 0)
            {
                perror("read");
                goto error;
            }
            buf[1] = '\0';

            if (strcmp(buf, "1") != 0 && strcmp(buf, "3") != 0)
            {
                FD_SET(sfd, &s);
                continue;
            }

            close(sfd);

            if (strcmp(buf, "1") == 0) // SIGHUP
            {
                // terminate customers' threads
            }
            else // SIGQUIT
            {
                // check until nCustomer equal 0
            }

            break;
        }
    }
 
    free(buf);

    ret = waitCashierExit();
    if (ret != 0)
    {
        perror("waitCashierExit");
        goto error;
    }

    // HERE ALL CUSTOMERS SHOULD BE CLOSED
    // HERE ALL CASHIERS SHOULD BE CLOSED

    ret = writeLogSupermarket();
    if (ret != 0)
    {
        perror("writeLogSupermarket");
        goto error;
    }
    
error:
    if (buf != NULL)
        free(buf);

    if (cashiers != NULL)
    {
        for (int i = 0; i < K; i++)
        {
            destroy_cashier(cashiers[i]);
        }

        free(cashiers);
    }

    // check all missing variables
    // TODO: NOTIFY DIRECTOR OF THE ERROR
    // close socket if open
}