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
#include "supermercato.h"
#include "cassa.h"
#include "cliente.h"

/*--- Definizione variabili globali ---*/
pthread_mutex_t aggClienti = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t notificaGestore = PTHREAD_COND_INITIALIZER;
int clientiSupermercato;

pthread_mutex_t gateClienti = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t uscitaClienti = PTHREAD_COND_INITIALIZER;
int richiestaUscita = 0;

pthread_mutex_t controlloSegnali = PTHREAD_MUTEX_INITIALIZER;
int flagQuit = 0;
int flagHup = 0;

pthread_cond_t chiusuraCassa = PTHREAD_COND_INITIALIZER;
int avvisoChiusuraCassa = 0;

int K, C, E, T, P;  
int tempoProdotto;  
int tempoAvvisoDir; 
int S1, S2;         

pthread_mutex_t occupato = PTHREAD_MUTEX_INITIALIZER;
casse_t **ptrCasseAperte;
int numCasseAperte;
casse_t *casse;

int ID = 1;
pthread_mutex_t accessoID = PTHREAD_MUTEX_INITIALIZER;

char *filenameLog;
FILE *fdLog;
pthread_mutex_t accessoFile = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
    char *res = NULL;
    char *val = NULL;
    filenameLog = (char *)malloc(31 * sizeof(char));
    FILE *fd;
    char buf[BUFSIZE];
    int numCasseIniziali = 0;
    int written = 0;
    char *str = (char *)malloc(BUFSIZE * sizeof(char));

    if (argc != 4)
    {
        fprintf(stdout, "Usage: %s <K> <C> <E>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    K = atoi(argv[1]);
    C = atoi(argv[2]);
    E = atoi(argv[3]);

    if (K < 1)
        LIMIT("K deve essere maggiore di 0\n");

    if (C < E + 1) 
        LIMIT("C deve essere almeno uguale a E+1 \n");

    if (E < 0 || E > C - 1) 
        LIMIT("E deve essere compreso tra 0 e C-1\n");

    /* Leggo le informazioni dal file config.txt */
    CHECK_EQ(fd = fopen("lib/config.txt", "r"), NULL, fopen);

    for (int i = 0; i < 8; i++)
    {
        CHECK_EQ(res = fgets(buf, BUFSIZE, fd), NULL, fgets);
        CHECK_EQ(val = strtok(res, ":"), NULL, strtok);
        CHECK_EQ(val = strtok(NULL, ":"), NULL, strtok);

        switch (i)
        {
        case 0:
            tempoProdotto = atoi(val);
            break;
        case 1:
            tempoAvvisoDir = atoi(val);
            break;
        case 2:
            S1 = atoi(val);
            break;
        case 3:
            S2 = atoi(val);
            break;
        case 4:
            numCasseIniziali = atoi(val);
            break;
        case 5:
            T = atoi(val);
            break;
        case 6:
            P = atoi(val);
            break;
        case 7:
            if (strlen(val) > 30)
                LIMIT("Nome del file di log troppo lungo\n");
            strcpy(filenameLog, strtok(val, " \n"));
            break;
        default:
            break;
        }
    }
    fclose(fd);

    if (numCasseIniziali < 1 || numCasseIniziali > K)
        LIMIT("Le casse iniziali devono essere un valore compreso tra 1 e K\n");
    if (tempoProdotto < 0 || tempoAvvisoDir < 0 || S1 < 0 || S2 < 0)
        LIMIT("Valori negativi in input non sono consentiti\n");
    if (T < 10)
        LIMIT("T deve essere maggiore di 9\n");
    if (P < 0)
        LIMIT("P deve essere un valore maggiore o uguale a 0\n");

    srand((unsigned)time(0));
    CHECK_EQ(fdLog = fopen(filenameLog, "w"), NULL, fopen);
    written = sprintf(str, "%d\n", K); // Scrivo il numero di cassieri nel file di log, per aiutarmi dopo con il parsing
    CHECK_NEQ(fwrite(str, 1, written, fdLog), written, fwrite);
    CHECK_NEQ(fclose(fdLog), 0, fclose);

    /* Alloco le casse (K) */
    CHECK_EQ(casse = (casse_t *)malloc(K * sizeof(casse_t)), NULL, malloc);

    for (int i = 0; i < K; i++)
    {
        casse[i].idCassa = i;
        casse[i].tempoGestFisso = rand() % (80 - 20 + 1) + 20;
        casse[i].codaClienti = initBQueue(C + 1);
        CHECK_NEQ(pthread_mutex_init(&casse[i].mutexDirettore, NULL), 0, pthread_mutex_init);
        casse[i].numClientiCoda = 0;
        CHECK_NEQ(pthread_mutex_init(&casse[i].mutexCassa, NULL), 0, pthread_mutex_init);
        casse[i].stato = 0;
        CHECK_NEQ(pthread_cond_init(&casse[i].svegliaCassa, NULL), 0, pthread_cond_init);
        casse[i].sumProd = 0;
        casse[i].sumClienti = 0;
        casse[i].numChiusure = 0;
        casse[i].ts_apertura.tv_nsec = 0, casse[i].ts_apertura.tv_sec = 0;
        casse[i].tAvgServizio = 0;
    }

    /* Avvio dei thread direttore, clienti, casse */
    pthread_mutex_lock(&occupato);
    pthread_t thDirett;
    CHECK_NEQ(pthread_create(&thDirett, NULL, Direttore, NULL), 0, pthread_create);

    // Aggiorno buffer e contatore delle casse aperte
    ptrCasseAperte = (casse_t **)malloc(K * sizeof(casse_t *));
    memset(ptrCasseAperte, 0 ,K*sizeof(casse_t*));
    numCasseAperte = numCasseIniziali;
    for (int i = 0; i < numCasseIniziali; i++)
    {
        pthread_t thCassa;
        casse[i].stato = 1; // aperta
        ptrCasseAperte[i] = &casse[i];
        CHECK_NEQ(pthread_create(&thCassa, NULL, Cassa, &casse[i]), 0, pthread_create);
        CHECK_NEQ(pthread_detach(thCassa), 0, pthread_detach);
    }
    pthread_mutex_unlock(&occupato);

    clientiSupermercato = C;
    for (int i = 0; i < C; i++)
    {
        pthread_t thCliente;
        CHECK_NEQ(pthread_create(&thCliente, NULL, Cliente, NULL), 0, pthread_create);
        CHECK_NEQ(pthread_detach(thCliente), 0, pthread_detach);
    }

    /* Avvio gestore clienti */
    pthread_t thGestClienti;
    CHECK_NEQ(pthread_create(&thGestClienti, NULL, GestoreClienti, NULL), 0, pthread_create);
    CHECK_NEQ(pthread_detach(thGestClienti), 0, pthread_detach);

    /* Gestisco i segnali che arriveranno */
    int sigrecv;
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    CHECK_EQ(sigwait(&set, &sigrecv), -1, sigwait);

    /* Se c'è un SIGHUP blocco l'entrata di altri clienti e il direttore chiuderà tutto
       una volta raggiunto quota zero di clienti nel supermercato*/
    if (sigrecv == 1) /* SIGHUP */
    {
        pthread_mutex_lock(&controlloSegnali);
        flagHup = 1;
        pthread_mutex_unlock(&controlloSegnali);
    }
    else if (sigrecv == 3) /* SIGQUIT */
    {
        pthread_mutex_lock(&controlloSegnali);
        flagQuit = 1;
        pthread_mutex_unlock(&controlloSegnali);
    }

    CHECK_EQ(pthread_join(thDirett, NULL), -1, pthread_join);

    /* Scrivo nel file di log i dati delle casse */
    pthread_mutex_lock(&accessoFile);
    CHECK_EQ(fdLog = fopen(filenameLog, "a"), NULL, fopen);
    for (int i = 0; i < K; i++)
    {
        pthread_mutex_lock(&casse[i].mutexCassa);
        
        if(casse[i].tAvgServizio != 0)
            // diviso 1000 perché voglio i "ms"
            casse[i].tAvgServizio = (casse[i].tAvgServizio / 1000) / casse[i].sumClienti; 

        written = sprintf(str, "| %5d | %5d | %5d | %5ld.%03ld | %5.3f | %5d |\n",
                          casse[i].idCassa, casse[i].sumProd,
                          casse[i].sumClienti, casse[i].ts_apertura.tv_sec,
                          casse[i].ts_apertura.tv_nsec % 1000,
                          casse[i].tAvgServizio,
                          casse[i].numChiusure);
        pthread_mutex_unlock(&casse[i].mutexCassa);
        
        CHECK_NEQ(fwrite(str, 1, written, fdLog), written, fwrite);
        pthread_mutex_unlock(&casse[i].mutexCassa);
    }
    CHECK_NEQ(fclose(fdLog), 0, fclose);
    pthread_mutex_unlock(&accessoFile);
    
    /* Libero la memoria alllocata */ 
    for (int i = 0; i < K; i++)
        deleteBQueue(casse[i].codaClienti, NULL);
    free(casse);
    free(filenameLog);
    free(ptrCasseAperte);
    free(str);

    pthread_exit(0);
}


void *Direttore(void *notused)
{
    /* Elementi di supporto */
    int contS1, contS2, nClienti, flag, min;
    int bound;
    int pos = 0;
    int stato = 1;
    int k = 0;
    casse_t *arg = NULL;
    casse_t *scelta = NULL;
    casse_t *daChiudere = NULL;
    struct timespec t;
    t.tv_nsec = 0;
    t.tv_sec = 1;

    while (1)
    {
        nanosleep(&t, 0); // tempo per limitare le aperture e chiusure frenetiche
        /* Non voglio che escano ulteriori clienti durante la gestione delle casse */
        pthread_mutex_lock(&gateClienti);
        richiestaUscita = 0;
        pthread_mutex_unlock(&gateClienti);
        
        /* Controllo segnali, se apro o chiudo una cassa non posso essere interrotto */
        pthread_mutex_lock(&controlloSegnali);
        flag = flagQuit;
        pthread_mutex_unlock(&controlloSegnali);

        pthread_mutex_lock(&occupato);
        contS1 = 0;
        contS2 = 0;
        daChiudere = NULL;
        min = C;
        pos = -1;
        for (int i = 0; i < numCasseAperte; i++)
        {
            scelta = ptrCasseAperte[i];
            pthread_mutex_lock(&scelta->mutexCassa);
            nClienti = scelta->numClientiCoda;
            pthread_mutex_unlock(&scelta->mutexCassa);
            if (nClienti <= 1)
                contS1 += 1;
            else if (nClienti >= S2)
                contS2 += 1;
            if (nClienti <= min)
            {
                daChiudere = scelta;
                min = nClienti;
                pos = i;
            }
        }

        if (!flag)
        {
            /* Se ho almeno S1 casse che hanno al più un cliente allora chiudo uno cassa */
            if (contS1 >= S1 && numCasseAperte > 1)
            {
                /* Chiudo una cassa, scelgo quella con più clienti */
                pthread_mutex_lock(&daChiudere->mutexCassa);
                daChiudere->stato = 0;
                pthread_cond_signal(&daChiudere->svegliaCassa);
                pthread_mutex_unlock(&daChiudere->mutexCassa);

                /* Aggiusto array */
                for (int i = pos; i < numCasseAperte-1; i++)
                    ptrCasseAperte[i] = ptrCasseAperte[i + 1];
                ptrCasseAperte[numCasseAperte-1] = NULL;
                numCasseAperte -= 1;

                /* Aspetto la chiusura della cassa, nel frattempo possono entrare altri clienti 
                   visto che le operazioni sulle casse sono concluse */
                while (!avvisoChiusuraCassa)
                    pthread_cond_wait(&chiusuraCassa, &occupato);

                avvisoChiusuraCassa = 0;
            }
            /* Se c'è almeno una cassa con almeno S2 clienti in coda allora apro una cassa */
            else if (contS2 >= 1 && numCasseAperte < K)
            {
                pthread_t nuovaCassa;
                arg = NULL;

                /* Prendo una cassa chiusa */
                while (arg == NULL)
                {
                    if (k == K)
                        k = 0;
                    if (casse[k].stato == 0)
                        arg = &casse[k];

                    k += 1;
                }

               
                /* Apro la cassa e aggiorno array delle casse aperte */
                ptrCasseAperte[numCasseAperte] = arg;
                numCasseAperte += 1;
                arg->stato = 1;
                arg->numClientiCoda = 0;
                CHECK_NEQ(pthread_mutex_init(&arg->mutexDirettore, NULL), 0, pthread_mutex_init);
                CHECK_NEQ(pthread_mutex_init(&arg->mutexCassa, NULL), 0, pthread_mutex_init);
                CHECK_NEQ(pthread_cond_init(&arg->svegliaCassa, NULL), 0, pthread_cond_init);

                CHECK_NEQ(pthread_create(&nuovaCassa, NULL, Cassa, arg), 0, pthread_create);
                CHECK_NEQ(pthread_detach(nuovaCassa), 0, pthread_detach);
            }
        }
        pthread_mutex_unlock(&occupato);

        pthread_mutex_lock(&gateClienti);
        richiestaUscita = 1;
        pthread_cond_broadcast(&uscitaClienti);
        pthread_mutex_unlock(&gateClienti);

        /* Controllo i segnali e lo stato dei clienti */
        pthread_mutex_lock(&controlloSegnali);
        if (flagQuit)
            stato = 2;
        pthread_mutex_unlock(&controlloSegnali);

        pthread_mutex_lock(&aggClienti);
        if (!clientiSupermercato)
            stato = 3;
        pthread_mutex_unlock(&aggClienti);

        if (stato != 1)
            break;
    }

    /* Se non ci sono più clienti o ricevo un segnale di chiusura (SIGQUIT), far uscire tutti i clienti */
    pthread_mutex_lock(&occupato);
    bound = numCasseAperte;
    for (int j = 0; j < bound; j++)
    {
        pthread_mutex_lock(&ptrCasseAperte[j]->mutexCassa);
        ptrCasseAperte[j]->stato = stato;
        pthread_cond_signal(&ptrCasseAperte[j]->svegliaCassa);
        pthread_mutex_unlock(&ptrCasseAperte[j]->mutexCassa);
        ptrCasseAperte[j] = NULL;
    }

    /* Dopo aver dato l'ordine di chiusura aspetto la chiusura effettiva delle casse,
       contando tutte le chiusure */
    while (numCasseAperte > 0)
        pthread_cond_wait(&chiusuraCassa, &occupato);
    pthread_mutex_unlock(&occupato);

    /* Sicurezza ulteriore */
    pthread_mutex_lock(&aggClienti);
    while (clientiSupermercato > 0)
        pthread_cond_wait(&notificaGestore, &aggClienti);
    pthread_mutex_unlock(&aggClienti);

    return NULL;
}

void *GestoreClienti(void *notused)
{
    while (1)
    {
        pthread_mutex_lock(&aggClienti);
        if (clientiSupermercato <= C - E)
        {
            pthread_mutex_lock(&controlloSegnali);
            if (flagHup || flagQuit)
            {
                pthread_mutex_unlock(&controlloSegnali);
                pthread_mutex_unlock(&aggClienti);
                break;
            }
            pthread_mutex_unlock(&controlloSegnali);

            /* Entrano altri E clienti */
            for (int i = 0; i < E; i++)
            {
                pthread_t thCliente;
                CHECK_NEQ(pthread_create(&thCliente, NULL, Cliente, NULL), 0, pthread_create);
                CHECK_NEQ(pthread_detach(thCliente), 0, pthread_detach);
                clientiSupermercato += 1;
            }
        }

        /* In attesa che escano E clienti */
        while (clientiSupermercato > C - E)
            pthread_cond_wait(&notificaGestore, &aggClienti);

        pthread_mutex_unlock(&aggClienti);
    }

    return NULL;
}
