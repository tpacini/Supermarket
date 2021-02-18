# Supermarket

## Dettagli di implementazione

***Supermercato:*** \
La struttura del programma è composta dal main, che agisce da supermercato, con al suo interno un thread direttore, un thread di supporto per gestire l’uscita e l’entrata dei clienti, K thread cassieri e C thread clienti.

</br>

***Direttore:*** \
All’avvio del Direttore viene effettuata una nanosleep di un secondo, necessaria a limitare l’apertura e la chiusura “schizofrenica” delle casse.
Dopodiché si analizza la situazione dei clienti in coda alle casse tramite un algoritmo, che “conta” il numero di casse che rispettano i vincoli imposti da S1 e S2 (parametri presenti nel file di configurazione) e che evidenzia la cassa con meno clienti.

La tecnica utilizzata per scegliere la cassa da aprire è questa:
<pre><code>
/* Prendo una cassa chiusa */
while (arg == NULL)
{
  if (k == K)
  k = 0;
  if (casse[k].stato == 0)
  arg = &casse[k];
  k += 1;
}
</code></pre>

In caso di **SIGQUIT**, il Direttore chiude le casse cambiando il loro stato, e attende la chiusura effettiva. Come ulteriore misura di sicurezza, la clientela viene avvisata della chiusura e anche in questo caso si attende l’uscita. 

In caso di **SIGHUP**, il Direttore continuerà con il suo operato e attuerà la stessa procedura descritta sopra solo quando tutti i clienti saranno usciti.

</br>

***Cassa:*** \
La cassa inizia la sua routine aspettando l’arrivo di clienti in coda, in questo tempo di attesa non avvisa il direttore sul numero di clienti poiché questo valore rimane invariato sostituibile con una *timedwait* in caso si voglia inviare comunque i dati al direttore durante l’attesa).

Dopo l’arrivo di un cliente, valuta il tempo necessario per gestire i prodotti acquistati e applica un algoritmo che calcola il tempo di gestione in modo dinamico in base al tempo che deve trascorrere prima di poter aggiornare nuovamente il numero di clienti in coda:
<pre><code>
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
</code></pre>

In caso di **SIGQUIT** la cassa si occupa di far uscire tutti i clienti e di avvisare il direttore una volta terminata la sua routine.

</br>

***Cliente:*** \
Il cliente dopo aver speso un tempo T a fare la spesa si mette in coda e aspetta il suo turno.
Dopo aver ricevuto un “segnale di sveglia” da parte della cassa, si rimette in attesa del cassiere che dovrà processare tutti i prodotti e una volta conclusa questa procedura, il cliente uscirà dal supermercato notificando il gestore della sua uscita.

In caso di **SIGQUIT**, lo stato del cliente (condTurno) viene modificato dal cassiere cosicché appena il cliente riceve la signal e ottiene la mutua esclusione, può uscire dal supermercato.

</br>

***GestoreClienti:*** \
Il gestore dei clienti utilizza la variabile “clientiSupermercato” per conoscere la situazione interna al supermercato.

Nel caso in cui questa variabile scenda sotto la soglia “*C-E*” allora il gestore si occupa di far partire E thread clienti e di aggiornare la variabile.

In caso di **SIGHUP o SIGQUIT**, il gestore interrompe la sua esecuzione.

## File di configurazione
Il file di configurazione contiene delle costanti utilizzate dal programma durante l’esecuzione:
- **tempo singolo prodotto** = tempo necessario per gestire un singolo prodotto
- **tempo avviso direttore** = intervallo di tempo trascorso prima di inviare un aggiornamento al direttore
- **S1** = se ci sono almeno S1 casse che hanno al più un cliente, una cassa viene chiusa
- **S2** = se c’è almeno una cassa con almeno S2 clienti in coda, una cassa può venir aperta
- **numero casse iniziali** = numero di casse che sono avviate inizialmente
- **T** = tempo massimo che un cliente può impiegare per fare gli acquisti
- **P** = numero di prodotti massimo che un cliente può acquistare
- **nome file log** = nome del file di log
I parametri che invece vengono passati come parametro sono:
- **K** = numero di cassieri nel supermercato
- **C** = numero di clienti che entrano all’avvio del supermercato
- **E** = numero di clienti che possono uscire prima di farne entrare altri E

## File di log
Nel file di log viene stampato inizialmente il numero di casse presenti nel supermercato, dopodiché appena un cliente esce dal supermercato inserisce i suoi dati (formattati), mentre i dati delle casse vengono inseriti dal supermercato prima di terminare (anch’essi formattati).

## Script di analisi (*analisi.sh*)
Lo script attende la terminazione del processo supermercato, dopodiché ottiene il nome del file di log dal file di configurazione.
Dal file di log ottiene il numero di righe che sono state scritte e, sapendo il numero di casse del supermercato ottiene il numero di linee relative al cliente (#righe - #casse). A questo punto viene fatta una read del file utilizzando tre condizioni:
- Se sto leggendo la prima riga allora vado avanti, poiché quest’ultima contiene il numero di casse
- Se sono arrivato alla linea “first” allora aggiungo una stringa che rappresenta la formattazione delle casse, infatti da questa riga in poi troviamo tutti i dati relativi alle casse 

Ad ogni iterazione, tranne che nella prima, stampo la riga sullo stdout

## Compilazione ed esecuzione
Per eseguire il programma, dopo aver compilato i file tramite “*make*”, si hanno due possibilità:
- ***make test***, per eseguire il test sui valori di default contenuti nel file *config.txt* all’interno della directory *lib*
- ***make cleanall***, per eliminare tutti i file generati dalla compilazione e il file di log


:warning: **PLEASE NOTE: The code contains some errors!!!** :warning:
