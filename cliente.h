#if !defined(CLIENTE_H)
#define CLIENTE_H

#include <stdlib.h>

#include <stdio.h>
#include <pthread.h>
#include <time.h>

/*--- Struttura cliente ---*/
typedef struct clienti
{
    int idCliente;          
    int tempoAcquisto;         // tempo necessario per l'acquisto dei prodotti
    int numProd;               // numero dei prodotti da acquistare
    int condTurno;             // variabile di stato per conoscere lo stato del cliente in coda
    pthread_cond_t turno;      // variabile di condizione tramite cui la cassa avvisa il cliente prima di gestirlo
    pthread_mutex_t mutex; 
} clienti_t;

extern int ID;                     // variabile utilizzata per conoscere l'ID del cliente appena entrato
extern pthread_mutex_t accessoID;

/**
 * @brief Dopo aver speso un certo tempo per acquistare i prodotti, si mette in coda in modo casuale
 *        in una delle casse aperte, dopodiché si mette in attesa del suo turno, se la cassa chiude
 *        mentre il cliente è ancora in coda, allora quest'ultimo si adatta alla situazione
 * 
 * @param notused 
 * @return void* 
 */
void *Cliente(void *notused);

/**
 * @brief Gestisce il tempo passato tra "start" e "end", correggendo delle dinamiche che comportavano
 *        la stampa di valori negativi relativi al tempo
 * 
 * @param start 
 * @param end 
 * @return struct timespec 
 */
extern struct timespec diff(struct timespec start, struct timespec end);

#endif