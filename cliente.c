#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include "lib/boundedqueue.h"
#include "lib/util.h"
#include "cliente.h"
#include "cassa.h"
#include "supermercato.h"

struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;

    if ((end.tv_nsec-start.tv_nsec) < 0)
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

void *Cliente(void *notused)
{
    /* Alloco strutture del cliente */
    clienti_t *cliente = (clienti_t *)malloc(sizeof(clienti_t));
    pthread_mutex_lock(&accessoID);
    cliente->idCliente = ID;
    srand((unsigned)ID);
    ID += 1;
    pthread_mutex_unlock(&accessoID);
    cliente->tempoAcquisto = rand() % (T - 10 + 1) + 10; 
    cliente->numProd = rand() % (P - 0 + 1);
    cliente->condTurno = 0;
    CHECK_NEQ(pthread_cond_init(&cliente->turno, NULL), 0, pthread_cond_init);
    CHECK_NEQ(pthread_mutex_init(&cliente->mutex, NULL), 0, pthread_mutex_init);
    /* Elementi di supporto */
    casse_t *cassaScelta = NULL;
    struct timespec t;
    int stato = 0;
    /* Elementi file di log */
    char *str = (char *)malloc(BUFSIZE * sizeof(char));
    int nCode = 0;
    struct timespec ts_start, ts_end, ts_temp, ts_coda;
    ts_coda.tv_nsec = 0;
    ts_coda.tv_sec = 0;
    struct timespec ts_start2, ts_end2, ts_super;
    int written = 0;


    clock_gettime(CLOCK_MONOTONIC, &ts_start2);
    t.tv_nsec = MSEC_TO_NSEC(cliente->tempoAcquisto);
    t.tv_sec = cliente->tempoAcquisto / 1000;
    nanosleep(&t, NULL);

    if (cliente->numProd != 0)
    {
        do
        {
            /* Dopo aver controllato che non ci sono zero casse aperte,
               il cliente sceglie una cassa in cui mettersi in coda */
            pthread_mutex_lock(&occupato);
            if (numCasseAperte == 0)
            {
                pthread_mutex_unlock(&occupato);
                break;
            }
            cassaScelta = ptrCasseAperte[rand() % (numCasseAperte - 0)];
            pthread_mutex_unlock(&occupato);

            /* Mi metto in coda e aspetto il mio turno */
            clock_gettime(CLOCK_MONOTONIC, &ts_start);
            nCode += 1;
            pthread_mutex_lock(&cassaScelta->mutexCassa);
            CHECK_EQ(push(cassaScelta->codaClienti, cliente), -1, push);
            // Se la cassa ha zero clienti e sta aspettando allora la avviso
            pthread_cond_signal(&cassaScelta->svegliaCassa);
            pthread_mutex_unlock(&cassaScelta->mutexCassa);
           
            /* I due while servono per gestire il tempo passato alla cassa, e per attendere 
               che i prodotti vengano processati */           
            pthread_mutex_lock(&cliente->mutex);
            while (cliente->condTurno == 0)
                pthread_cond_wait(&cliente->turno, &cliente->mutex);

            clock_gettime(CLOCK_MONOTONIC, &ts_end);
            ts_temp = diff(ts_start, ts_end);
            ts_coda.tv_sec += ts_temp.tv_sec;
            ts_coda.tv_nsec += ts_temp.tv_nsec;

            while (cliente->condTurno == 1)
                pthread_cond_wait(&cliente->turno, &cliente->mutex);

            stato = cliente->condTurno + 1;
            pthread_mutex_unlock(&cliente->mutex);
            cliente->condTurno = 0;
        } while (stato == 2);
    }
    else
    {
        /* Ho zero prodotti, attendo  la conferma di poter uscire
           da parte del direttore */
        pthread_mutex_lock(&gateClienti);
        while (!richiestaUscita)
            pthread_cond_wait(&uscitaClienti, &gateClienti);
        pthread_mutex_unlock(&gateClienti);
    }
    clock_gettime(CLOCK_MONOTONIC, &ts_end2);
    ts_super = diff(ts_start2, ts_end2);
    

    /* Scrivo nel file di log */
    written = sprintf(str, "| %5d | %5d | %5ld.%03ld | %5ld.%03ld | %5d |\n",
                      cliente->idCliente, cliente->numProd, ts_super.tv_sec, 
                      ts_super.tv_nsec % 1000, ts_coda.tv_sec, ts_coda.tv_nsec % 1000, nCode);
    pthread_mutex_lock(&accessoFile);
    CHECK_EQ(fdLog = fopen(filenameLog, "a"), NULL, fopen);
    CHECK_NEQ(fwrite(str, 1, written, fdLog), written, fwrite);
    CHECK_NEQ(fclose(fdLog), 0, fclose);
    pthread_mutex_unlock(&accessoFile);

    pthread_mutex_lock(&aggClienti);
    clientiSupermercato -= 1;
    pthread_cond_signal(&notificaGestore);
    pthread_mutex_unlock(&aggClienti);
    
    free(str);
    free(cliente);
    return NULL;
}

