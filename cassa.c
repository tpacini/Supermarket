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


void *Cassa(void *arg)
{
    /* Elementi della struct Cassa*/
    BQueue_t *coda = ((casse_t *)arg)->codaClienti;
    /* Elementi di supporto*/
    clienti_t *clienteServito = NULL;
    struct timespec t;
    t.tv_sec = 0;
    int tElapsed = 0;
    int tTot = 0;
    int tFisso = ((casse_t *)arg)->tempoGestFisso;
    int prodotti = 0;
    int stato = 1;
    /* Elementi del file di log */
    struct timespec ts_start, ts_end, ts_temp;
    int tServ = 0;


    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    do
    {
        /* Attendo l'arrivo di un cliente o il cambio di stato*/
        pthread_mutex_lock(&((casse_t *)arg)->mutexCassa); 
        while ((stato = ((casse_t *)arg)->stato) == 1 && getLength(coda) == 0)
            pthread_cond_wait(&((casse_t *)arg)->svegliaCassa, &((casse_t *)arg)->mutexCassa);
        pthread_mutex_unlock(&((casse_t *)arg)->mutexCassa);


        if (stato == 1 && getLength(coda) != 0)
        {
            /* Estraggo un cliente dalla coda e lo avviso */
            clienteServito = pop(coda);
            pthread_mutex_lock(&clienteServito->mutex);
            prodotti = clienteServito->numProd;
            ((casse_t *)arg)->sumProd += prodotti;
            ((casse_t *)arg)->sumClienti += 1;
            clienteServito->condTurno = 1;
            pthread_cond_signal(&clienteServito->turno);
            pthread_mutex_unlock(&clienteServito->mutex);

            /* L'algoritmo si adatta in modo dinamico al tempo di avviso direttore, in questo modo spendo
               un tempo minimo per inviare i dati al direttore per poi continuare a servire il cliente */
            tTot = tFisso + tempoProdotto * prodotti; // tempo totale per gestire il cliente
            tServ = tTot;
            tElapsed = tempoAvvisoDir - tElapsed;
            while (tTot >= tElapsed)
            {
                t.tv_nsec = MSEC_TO_NSEC(tElapsed);
                t.tv_sec = tElapsed / 1000;
                nanosleep(&t, NULL);

                pthread_mutex_lock(&((casse_t *)arg)->mutexDirettore);
                ((casse_t *)arg)->numClientiCoda = getLength(coda);
                pthread_mutex_unlock(&((casse_t *)arg)->mutexDirettore);

                /* Tempo di servizio rimanente: tTot-tElapsed*/
                if (tTot == tElapsed)
                {
                    tElapsed = 0;
                    break;
                }
                tTot = tTot - tElapsed;
            }
            /* Per concludere la gestione del cliente deve trascorrere un tempo tTot */
            if (tTot > 0 && tTot < tempoAvvisoDir)
            {
                tElapsed = tTot; // tempo trascorso del tempo di avviso del direttore
                t.tv_nsec = MSEC_TO_NSEC(tTot);
                t.tv_sec = tTot / 1000;
                nanosleep(&t, NULL);
            }

            /* Avviso il cliente che può lasciare la cassa */
            pthread_mutex_lock(&clienteServito->mutex);
            clienteServito->condTurno += 1;
            pthread_cond_signal(&clienteServito->turno);
            pthread_mutex_unlock(&clienteServito->mutex);

            /* Controllo se il direttore ha chiuso la cassa */
            pthread_mutex_lock(&((casse_t *)arg)->mutexCassa);
            stato = ((casse_t *)arg)->stato;
            ((casse_t *)arg)->tAvgServizio += tServ;
            pthread_mutex_unlock(&((casse_t *)arg)->mutexCassa);
        }
    } while (stato == 1);
    clock_gettime(CLOCK_MONOTONIC, &ts_end);

    /* Se la cassa è stata chiusa, preparo un flag per ogni cliente che indica se quest'ultimo
       deve cambiare cassa o deve uscire immediatamente dal supermercato */
    pthread_mutex_lock(&((casse_t *)arg)->mutexCassa);
    while (getLength(coda) != 0)
    {
        clienteServito = pop(coda);
        pthread_mutex_lock(&clienteServito->mutex);
        clienteServito->condTurno = stato + 2; // il "+2" serve per rispettare le guardie del cliente
        pthread_cond_signal(&clienteServito->turno);
        pthread_mutex_unlock(&clienteServito->mutex);
    }
    pthread_mutex_unlock(&((casse_t *)arg)->mutexCassa);

     /* Avviso il direttore della mia uscita */
    if (stato == 0)
    {
        pthread_mutex_lock(&occupato);
        avvisoChiusuraCassa = 1;
        pthread_cond_signal(&chiusuraCassa);
        pthread_mutex_unlock(&occupato);
    }
    else if (stato == 2 || stato == 3)
    {
        pthread_mutex_lock(&occupato);
        numCasseAperte -= 1;
        pthread_cond_signal(&chiusuraCassa);
        pthread_mutex_unlock(&occupato);
    }

    /* Valori del file di log */
    ((casse_t *)arg)->numChiusure += 1;
    ts_temp = diff(ts_start, ts_end);
    ((casse_t *)arg)->ts_apertura.tv_nsec += ts_temp.tv_nsec;
    ((casse_t *)arg)->ts_apertura.tv_sec += ts_temp.tv_sec;

    return NULL;
}
