#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#include "glob.h"
#include "main.h"
#include "customer.h"


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
    FILE* fp;
    char* buf, *tok;
    int sfd, ret;
    
    unsigned int nCashier;

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

    // Parse nCashier from config
    if (fseek(fp, 1L, SEEK_SET) == -1)
    {
        perror("fseek");
        thread_mutex_unlock(&configAccess);
        goto error;
    }
    if (fread(buf, sizeof(char), MAX_LINE, fp) == 0)
    {
        perror("fread");
        thread_mutex_unlock(&configAccess);
        goto error;
    }
    fclose(fp);

    buf = (char*) malloc(MAX_LINE*sizeof(char));
    if (buf == NULL)
    {
        perror("malloc");
        pthread_mutex_lock(&configAccess);
        goto error;
    }

    tok = strtok(buf, " ");
    while (tok != NULL)
    {
        // Next element is nCashier
        if (strcmp(tok, ":") == 0)
        {
            tok = strtok(NULL, " ");
            nCashier = convert(tok);
            if (nCashier <= 0 || nCashier > K)
            {
                perror("Number of initial cashiers should be higher \
                        than 0 and lower than K");
                thread_mutex_unlock(&configAccess);
                goto error;
            }
        }
        else
            tok = strtok(NULL, " ");
    }

    free(buf);
    thread_mutex_unlock(&configAccess);

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
    for (int i = 0; i < nCashier; i++)
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

    // read ... wait for signal

    // close socket


    // Write to log
    // write supermarket data
    // write cashier's data

error:
    if (buf != NULL)
        free(buf);
    if (ftell(fp) >= 0)
        fclose(fp);
    if (cashiers != NULL)
    {
        for (int i = 0; i < K; i++)
        {
            destroy_cashier(cashiers[i]);
        }

        free(cashiers);
    }

    // TODO: NOTIFY DIRECTOR OF THE ERROR
    // close socket if open
}