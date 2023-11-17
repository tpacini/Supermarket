#if !defined(BOUNDED_QUEUE_H)
#define BOUNDED_QUEUE_H

#include <pthread.h>

/** Struttura dati coda.
 *
 */
typedef struct BQueue
{
    void **buf;
    size_t head;
    size_t tail;
    size_t qsize; // size of the queue
    size_t qlen;  // actual lenght
    pthread_mutex_t m;
    pthread_cond_t cfull;
    pthread_cond_t cempty;
} BQueue_t;

/** Alloca ed inizializza una coda di dimensione n.
 *  Deve essere chiamata da un solo thread (tipicamente il thread main).
 *
 *   \param n lunghezza della coda
 *   \retval NULL se si sono verificati problemi nell'allocazione (errno settato)
 *   \retval q puntatore alla coda allocata
 */
BQueue_t *initBQueue(size_t n);

/** Cancella una coda allocata con initQueue. Deve essere chiamata da
 *  da un solo thread (tipicamente il thread main).
 *  
 *   \param q puntatore alla coda da cancellare
 */
void deleteBQueue(BQueue_t *q, void (*F)(void *));

/** Inserisce un dato nella coda.
 *   \param q puntatore alla coda 
 *   \param data puntatore al dato da inserire
 *  
 *   \retval  0 se successo
 *   \retval -1 se errore (errno settato opportunamente)
 */
int push(BQueue_t *q, void *data);

/** Estrae un dato dalla coda.
 *
 *  \retval data puntatore al dato estratto.
 */
void *pop(BQueue_t *q);

/** Restituisce il numero di elementi in coda.
 *  \param q puntatore alla coda 
 *
 *  \retval n = numero di elementi.
 */
int getLength(BQueue_t *q);

#endif /* BOUNDED_QUEUE_H */
