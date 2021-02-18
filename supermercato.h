#if !defined(SUPERMERCATO_H)
#define SUPERMERCATO_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>


#define LIMIT(str)            \
    {                         \
        fprintf(stderr, str); \
        exit(EXIT_FAILURE);   \
    }   

extern int K, C, E, T, P;  // valori di input
extern int tempoProdotto;  // tempo necessario per processare un prodotto
extern int tempoAvvisoDir; // intervallo per avviso direttore
extern int S1, S2;         // soglia chiusura/apertura cassa

extern pthread_cond_t chiusuraCassa;     // avviso chiusura cassa
extern int avvisoChiusuraCassa;          // condizione del while su attesa cassa

extern pthread_mutex_t aggClienti;       // accedere al numero di clienti
extern pthread_cond_t notificaGestore;   // avvisare gestore clienti di uscita cliente
extern int clientiSupermercato;          // numero di clienti all'interno del supermercato

extern pthread_mutex_t gateClienti;      // mutua esclusione su attesa conferma del direttore
extern pthread_cond_t uscitaClienti;     // sblocco clienti con zero prodotti
extern int richiestaUscita;              // condizione del while su attesa direttore


extern pthread_mutex_t controlloSegnali; // mutua esclusione su accesso ai flag
extern int flagQuit;                     // flag di SIGQUIT
extern int flagHup;                      // flag di SIGHUP

extern char* filenameLog;                // file path del file di log
extern FILE* fdLog;                      // accesso al file log
extern pthread_mutex_t accessoFile;      // mutua esclusione su accesso al file di log


/**
 * @brief Il direttore gestisce la chiusura e l'apertura delle casse tramite dati, riguardanti la
 *        quantità di clienti in coda ad una cassa, che vengono aggiornati costantemente
 * 
 * @param notused 
 * @return void* 
 */
void *Direttore(void *notused);

/**
 * @brief La funzione si occupa di far entrare nel supermercato nuovi clienti, dopo l'uscita di E clienti,
 *        dove E è un valore specificato nel file di configurazione
 * 
 * @param notused 
 * @return void* 
 */
void *GestoreClienti(void *notused);

#endif