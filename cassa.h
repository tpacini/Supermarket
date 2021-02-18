#if !defined(CASSA_H)
#define CASSA_H

#include<pthread.h>
#include "lib/boundedqueue.h"
#include <time.h>

/*--- Definizione cassa ---*/
typedef struct cassa
{
    int idCassa;                     // ID
    int tempoGestFisso;              // tempo di gestione del cassiere
    BQueue_t *codaClienti;           // coda (limitata) di clienti
    pthread_mutex_t mutexDirettore;  
    int numClientiCoda;              // utilizzata dal direttore per il suo algoritmo di chiusura 
    pthread_mutex_t mutexCassa;      // mutua esclusione per gran parte della "struttura"
    int stato;                       // stato corrente della cassa
    pthread_cond_t svegliaCassa;     
    int sumProd;                     // numero di prodotti processati
    int sumClienti;                  // numero di clienti serviti
    int numChiusure;                 // numero di chiusure effettuate
    struct timespec ts_apertura;     // tempo di apertura della cassa
    float tAvgServizio;              // tempo medio di servizio
} casse_t;


extern pthread_mutex_t occupato;  // mutua esclusione sui dati in tempo reale di tutte le casse aperte
extern casse_t **ptrCasseAperte;  // puntatori di tutte le casse aperte
extern int numCasseAperte;        // numero di casse aperte

extern casse_t *casse;            // elenco di tutte le casse disponibili (chiuse e aperte)

/**
 * @brief La cassa si mette in attesa di un cliente, appena arriva lo gestisce e nel frattempo
 *        gestisce l'invio di dati aggiornati riguardo il numero di clienti in coda. La cassa
 *        può essere chiusa tramite l'accesso alla variabile di stato e di conseguenza verrano
 *        avvisati i clienti sulla causa della chiusura (allarme o chiusura causa pochi clienti in coda)
 * 
 * @param arg 
 * @return void* 
 */
void *Cassa(void *arg);

#endif