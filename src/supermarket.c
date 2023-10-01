#define _POSIX_C_SOURCE 199506L

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "supermarket.h"
#include "glob.h"

#define SIGHUP_STR "1"
#define SIGQUIT_STR "3"
#define SIGTERM 15

unsigned int K, C, E, T, P, S;
Cashier_t **cashiers;
Customer_t **customers;
pthread_mutex_t logAccess;
unsigned int currentNCustomer;
unsigned int totNCustomer;
unsigned int totNProd;
pthread_mutex_t numCu;

pthread_mutex_t configAccess;

unsigned int parseNfc()
{
    FILE *fp;
    char *tok, *buf;
    unsigned int nfc = 0;

    buf = (char *)malloc(MAX_LINE * sizeof(char));
    if (buf == NULL)
    {
        perror("parseNfc: malloc");
        return 0;
    }

    pthread_mutex_lock(&configAccess);
    fp = fopen(CONFIG_FILENAME, "r");
    if (fp == NULL)
    {
        perror("parseNfc: fopen");
        pthread_mutex_unlock(&configAccess);
        free(buf);
        return 0;
    }
    // Second row
    if (fseek(fp, 1L, SEEK_SET) == -1)
    {
        perror("parseNfc: fseek");
        pthread_mutex_unlock(&configAccess);
        fclose(fp);
        free(buf);
        return 0;
    }

    if (fread(buf, sizeof(char), MAX_LINE, fp) == 0)
    {
        perror("parseNfc: fread");
        pthread_mutex_unlock(&configAccess);
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
            errno = 0;
            nfc = strtoul(tok, NULL, 10);
            if (errno == EINVAL || errno == ERANGE)
            {
                fprintf(stderr, "parseNfc: strtoul: %d", errno);
                pthread_mutex_unlock(&configAccess);
                free(buf);
                return 0;
            }
            if (nfc > K)
            {
                perror("parseNfc: number of initial cashiers should be higher \
                        than 0 and lower than K");
                pthread_mutex_unlock(&configAccess);
                free(buf);
                return 0;
            }
        }
        else
            tok = strtok(NULL, " ");
    }

    free(buf);
    pthread_mutex_unlock(&configAccess);

    if (DEBUG)
        printf("nfc: %d", nfc);

    return nfc;
}

int writeLogSupermarket()
{
    char *logMsg;
    unsigned int to, mst;
    FILE* fp;

    // No need to mutex, all the threads terminated
    fp = fopen(LOG_FILENAME, "a");
    if (fp == NULL)
    {
        perror("writeLogSupermarket: fopen");
        return -1;
    }

    // Write supermarket data
    logMsg = (char *)malloc((10 * 2 + 2 + 1) * sizeof(char));
    if (logMsg == NULL)
    {
        perror("writeLogSupermarket: malloc");
        fclose(fp);
        return -1;
    }
    sprintf(logMsg, "%10u %10u\n", totNCustomer, totNProd);
    fwrite(logMsg, sizeof(char), strlen(logMsg), fp);
    free(logMsg);

    // Write cashier's data
    logMsg = (char *)malloc((10 * 5 + 5 + 1) * sizeof(char));
    if (logMsg == NULL)
    {
        perror("writeLogSupermarket: malloc");
        fclose(fp);
        return -1;
    }

    // Write cashiers data
    for (int i = 0; i < K; i++)
    {
        memset(logMsg, 0, strlen(logMsg));

        // No need to mutex, all threads terminated
        to = timespec_to_ms(cashiers[i]->timeOpen);
        mst = timespec_to_ms(cashiers[i]->meanServiceTime);

        sprintf(logMsg, "%10u %10u %10u %10u %10u\n", cashiers[i]->totNProds, 
                    cashiers[i]->totNCustomer, to, 
                    cashiers[i]->nClose, mst);
        fwrite(logMsg, sizeof(char), strlen(logMsg), fp);
    }

    free(logMsg);
    fclose(fp);

    return 0;
}

int waitCustomerTerm()
{
    unsigned int c = 0;
    struct timespec timeout;

    clock_gettime(CLOCK_MONOTONIC, &timeout);
    timeout.tv_sec = 0;
    timeout.tv_nsec = 100 * 1000000; // 100 ms

    while (true)
    {
        pthread_mutex_lock(&numCu);
        c = currentNCustomer;
        pthread_mutex_unlock(&numCu);
        
        if (c == 0)
            break;
        else
        {
            if (nanosleep(&timeout, NULL) != 0)
            {
                perror("waitCustomerTerm: nanosleep");
                return -1;
            }
        }
    }

    return 0;
}

int waitCashierTerm()
{
    int empty = 0;
    void* nullCu = NULL;
    struct timespec timeout;

    clock_gettime(CLOCK_MONOTONIC, &timeout);
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
                    perror("waitCashierTerm: nanosleep");
                    return -1;
                }
            }
        }

        empty = 0;
    }

    return 0;
}

int enterCustomers()
{
    int ret, index = 0, running;
    Customer_t *c = NULL;

    // Start E customer threads
    if (currentNCustomer <= C-E)
    {
        pthread_mutex_lock(&numCu);
        currentNCustomer += E;
        pthread_mutex_lock(&numCu);
        for (int i = 0; i < E; i++)
        {
            pthread_t thCu;

            // Find unused data structure
            for (int j = index; j < C; j++)
            {
                pthread_mutex_lock(&customers[j]->accessState);
                if (customers[j]->running == 0)
                    running = 0;
                else
                    running = 1;
                pthread_mutex_unlock(&customers[j]->accessState);

                if(!running)
                {
                    c = customers[i];

                    // Reset data
                    c->productProcessed = false;
                    c->yourTurn = false;
                    c->nProd = rand() % (P - 0 + 1);
                    c->running = 1;
                    c->id = thCu;

                    break;
                }
            }

            if (c == NULL)
            {
                perror("enterCustomers: no free structure found.");
                return -1;
            }

            ret = pthread_create(&thCu, NULL, (void*)CustomerP, c);
            if (ret != 0)
            {
                perror("enterCustomers: pthread_create");
                return -1;
            }
            if (pthread_detach(thCu) != 0)
            {
                perror("enterCustomers: pthread_detach");
                return -1;
            }
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    char* buf = NULL;
    int sfd, ret;
    unsigned int nFirstCashier;  // number of cashier to start at time 0

    /* Variables for "select" */
    fd_set s;
    struct timeval timeout;

    currentNCustomer = 0;
    totNCustomer = 0;
    totNProd = 0;
    if (pthread_mutex_init(&numCu, NULL) != 0 ||
        pthread_mutex_init(&logAccess, NULL) != 0)
    {
        perror("supermarket: pthread_mutex_init");
        goto error;
    }

    if (argc != 7)
    {
        fprintf(stdout, "Usage: %s <K> <C> <E> <T> <P> <S>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    errno = 0;
    ret = 1;
    K = strtoul(argv[1], NULL, 10); // max. number of cashiers
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;
    C = strtoul(argv[2], NULL, 10); // max. number of customers inside supermarket
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;
    E = strtoul(argv[3], NULL, 10); // number of customers to enter supermarket simultaneously
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;
    T = strtoul(argv[4], NULL, 10); // max. time to buy products
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;
    P = strtoul(argv[5], NULL, 10); // max. number of products
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;
    S = strtoul(argv[6], NULL, 10); // time after customer looks for other cashiers
    ret *= (errno == EINVAL || errno == ERANGE) ? 0 : 1;

    if (ret == 0)
    {
        perror("supermarket: error converting arguments");
        goto error;
    }

    if (K <= 0 || C <= 0 || T <= 0 || S <= 0 || P <= 0)
    {
        fprintf(stderr, "Parameters should be greater than 0\n");
        exit(EXIT_FAILURE);
    }
    if (!(E > 0 && E < C))
    {
        fprintf(stderr, "E should be greater than 0 and lower than C \n");
        exit(EXIT_FAILURE);
    }

    // Parse nFirstCashier from config
    nFirstCashier = parseNfc();
    if (nFirstCashier == 0)
    {
        perror("supermarket: parseNfc");
        goto error;
    }

    pthread_mutex_lock(&numCu);
    currentNCustomer = 0;
    pthread_mutex_unlock(&numCu);

    // Initialize cashiers' data
    cashiers = (Cashier_t**) malloc(K*sizeof(Cashier_t*));
    if (cashiers == NULL)
    {
        perror("supermarket: malloc");
        goto error;
    }
    for (int i = 0; i < K; i++)
    {
        if(init_cashier(cashiers[i]) == -1)
        {
            perror("supermarket: init_cashier");
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
            perror("supermarket: pthread_create");
            goto error;
        }
        if (pthread_detach(thCa) != 0)
        {
            perror("supermarket: pthread_detach");
            goto error;
        }
    }

    // Initialize customers' data
    customers = (Customer_t **)malloc(C * sizeof(Customer_t *));
    if (customers == NULL)
    {
        perror("supermarket: malloc");
        goto error;
    }
    for (int i = 0; i < C; i++)
    {
        if (init_customer(customers[i]) == -1)
        {
            perror("supermarket: init_customer");
            goto error;
        }
    }

    // Start customers' threads (C)
    pthread_mutex_lock(&numCu);
    currentNCustomer = C;
    pthread_mutex_unlock(&numCu);
    for (int i = 0; i < C; i++)
    {
        pthread_t thCu;
        pthread_mutex_lock(&customers[i]->accessState);
        customers[i]->running = 1;
        customers[i]->id = thCu;
        pthread_mutex_unlock(&customers[i]->accessState);

        ret = pthread_create(&thCu, NULL, CustomerP, customers[i]);
        if (ret != 0)
        {
            perror("supermarket: pthread_create");
            goto error;
        }
        if (pthread_detach(thCu) != 0)
        {
            perror("supermarket: pthread_detach");
            goto error;
        }
    }

    if (DEBUG)
        printf("Customers and Cashiers have been started.\n");

    // Handling SIGHUP or SIGQUIT from Director
    sfd = socket(AF_UNIX, SOCK_STREAM, 0); // ip protocol
    if (sfd == -1)
    {
        perror("supermarket: socket");
        goto error;
    }
    ret = connect(sfd, NULL, 0);
    if (ret == -1)
    {
        perror("supermarket: connect");
        goto error;
    }

    if (DEBUG)
        printf("Connected to socket.\n");

    buf = (char*) malloc((strlen(SIGHUP_STR)+1)*sizeof(char));
    if (buf == NULL)
    {
        perror("supermarket: malloc");
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
            perror("supermarket: select");
            goto error;
        }
        // timeout expired
        else if (ret == 0)
        {
            ret = enterCustomers();
            if (ret == -1)
            {
                perror("supermarket: enterCustomers");
                goto error;
            }
        }
        // read ready
        else
        {
            if (read(sfd, buf, strlen(SIGHUP_STR)) < 0)
            {
                perror("supermarket: read");
                goto error;
            }
            buf[strlen(SIGHUP_STR)] = '\0';

            if (strcmp(buf, SIGHUP_STR) != 0 && strcmp(buf, SIGQUIT_STR) != 0)
            {
                FD_SET(sfd, &s);
                continue;
            }

            close(sfd);

            // Terminate customers' threads
            if (strcmp(buf, SIGQUIT_STR) == 0)
            {
                if (DEBUG)
                    printf("Read SIGQUIT signal.\n");

                for (int i = 0; i < C; i++)
                {
                    ret = pthread_kill(customers[i]->id, SIGTERM);
                    if (ret != 0)
                    {
                        perror("supermarket: pthread_kill");
                        goto error;
                    }
                    destroy_customer(customers[i]);
                }

                free(customers);
            }
            else // wait customer termination
            {
                if (DEBUG)
                    printf("Read SIGHUP signal.\n");
                
                ret = waitCustomerTerm();
                if (ret != 0)
                {
                    perror("supermarket: waitCustomerTerm");
                    goto error;
                }
            }

            break;
        }
    }
 
    free(buf);

    ret = waitCashierTerm();
    if (ret != 0)
    {
        perror("supermarket: waitCashierTerm");
        goto error;
    }

    if (DEBUG)
        printf("Customers and Cashiers closed.\n");
        
    ret = writeLogSupermarket();
    if (ret != 0)
    {
        perror("supermarket: writeLogSupermarket");
        goto error;
    }

    if (DEBUG)
        printf("Saved information to log.\n");
    
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

    if (customers != NULL)
        free(customers); 

    // check all missing variables (also customers, cashiers,..)
    // TODO: NOTIFY DIRECTOR OF THE ERROR
    // close socket if open

    // TODO: different seed for every thread which used rand_r
    // TODO: fix log data due to errors in project's requirements

    if (DEBUG)
        printf("Closing Supermarket...\n");
}