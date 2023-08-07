#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#pragma once

#define MAX_LINE 50
#define LOG_FILENAME "./log.txt"
#define CONFIG_FILENAME "./lib/config.txt"
#define SUPMRKT_EXEC_PATH "./supermarket"

/* Mutex to access log file */
pthread_mutex_t logAccess;

/* Mutex to access configuration file*/
pthread_mutex_t configAccess;


unsigned int currentNCustomer;
unsigned int totNCustomer;
unsigned int totNProd;
pthread_mutex_t numCu;
 

unsigned int convert(char *st);

/* Perform the difference between two timespec elements. Return
   a timespec struct with the result. */
struct timespec diff(struct timespec start, struct timespec end);

/* Convert milliseconds into timespec data structure. Return 
   the converted data. */
struct timespec ms_to_timespec(unsigned int milliseconds);

/* Perform the sum between two timespec elements. Return
   a timespec struct with the result. */
struct timespec add_ts(struct timespec a, struct timespec b);

struct timespec mean_ts(struct timespec tot, unsigned int n);

unsigned int timespec_to_ms(struct timespec ts);