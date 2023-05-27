#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cashier.h"
#include "glob.h"

#define MAX_LINE 50
#define PROD_THRESH 1000
#define NOTIFY_TRESH 5000

unsigned int convert(char *st)
{
    char *x;
    for (x = st; *x; x++)
    {
        if (!isdigit(*x))
            return NULL;
    }
    return (strtoul(st, NULL, 10));
}

void CashierP()
{
    unsigned char *buf, *tok;
    FILE *fp;

    unsigned int procTime = rand() % (80 - 20 + 1) + 20;   // processing time
    unsigned int timeProd = 0;                             // time to process single product
    unsigned int timeNotify = 1000;                        // delay to notify director

    Cashier_t* ca = (Cashier_t*) malloc(sizeof(Cashier_t));
    if (ca == NULL)
    {
        perror("malloc");
        goto error;
    }
    if (pthread_mutex_init(&ca->accessQueue, NULL) != 0)
    {
        perror("pthread_mutex_init");
        goto error;
    }
    // If all customers inside the supermarket are in this line
    ca->queueCustomers = initBQueue(C);

    // Retrieve parameters from configuration file
    pthread_mutex_lock(&configAccess);
    fp = fopen('lib/config.txt', 'r');
    if (fp == NULL)
    {
        perror("fopen");
        pthread_mutex_unlock(&configAccess);
        goto error;
    }

    buf = (char*) malloc(MAX_LINE*sizeof(char));
    if (buf == NULL)
    {
        perror("malloc");
        thread_mutex_unlock(&configAccess);
        goto error;
    }

    if (fseek(fp, 0L, SEEK_SET) == -1)
    {
        perror("fseek");
        thread_mutex_unlock(&configAccess);
        goto error;
    }
    if (fread(buf, sizeof(char), MAX_LINE, fp) == 0)
    {
        perror("fseek");
        thread_mutex_unlock(&configAccess);
        goto error;
    }
    tok = strtok(buf, " ");
    while (tok != NULL)
    {
        // Next element is timeProd
        if (strcmp(tok, ":") == 0)
        {
            tok = strtok(NULL, " ");
            timeProd = convert(tok);
            if (timeProd > PROD_THRESH)
            {
                perror("Time to process single product too large");
                thread_mutex_unlock(&configAccess);
                goto error;
            }
        }
        else
            tok = strtok(NULL, " ");
        
    }

    memset(buf, 0, strlen(buf));

    if (fseek(fp, 1L, SEEK_SET) == -1)
    {
        perror("fseek");
        thread_mutex_unlock(&configAccess);
        goto error;
    }
    if (fread(buf, sizeof(char), MAX_LINE, fp) == 0)
    {
        perror("fseek");
        thread_mutex_unlock(&configAccess);
        goto error;
    }
    tok = strtok(buf, " ");
    while (tok != NULL)
    {
        // Next element is timeNotify
        if (strcmp(tok, ":") == 0)
        {
            tok = strtok(NULL, " ");
            timeNotify = convert(tok);
            if (timeNotify > NOTIFY_TRESH)
            {
                perror("Delay to notify director too large");
                thread_mutex_unlock(&configAccess);
                goto error;
            }
        }
        else
            tok = strtok(NULL, " ");
    }

    free(buf);
    fclose(fp);
    pthread_mutex_unlock(&configAccess);

    // Start handling customers


    // Periodically notify director

    // TODO: FINISH CASHIER
    
    // Nel loop del cassiere prima di servire un altro cliente, il cassiere controlla se il direttore ha chiuso la cassa

error:
    if (ca->queueCustomers != NULL)
        deleteBQueue(ca->queueCustomers, NULL);
    if (ca != NULL)
        free(ca);
    if (buf != NULL)
        free(buf);
    if (ftell(fp) >= 0)
        fclose(fp);


}