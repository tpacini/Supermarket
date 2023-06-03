#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>

#include "glob.h"

int main(int argc, char* argv[])
{   
    FILE* fp;
    char* buf, *tok;
    
    unsigned int nCashier;

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
            if (nCashier > K)
            {
                perror("Number of initial cashiers too high");
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
    // Allocate Cashiers
    // Free cashiers before leaving

    // Define array of cashiers

    /*
    Cashier_t* ca = (Cashier_t*) malloc(sizeof(Cashier_t));
    if (ca == NULL)
    {
        perror("malloc");
        goto error;
    }
    if (cashier_init(ca) != 0)
    {
        perror("cashier_init");
        goto error;
    }
    */

    // Start cashiers' threads

    // Start customers' threads (C)



    // Handling SIGHUP or SIGQUIT from Director


    // Write to log

error:
    if (buf != NULL)
        free(buf);
    if (ftell(fp) >= 0)
        fclose(fp);
    
}