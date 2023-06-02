#include <pthread.h>
#include <stdbool.h>
#include <time.h>

#pragma once

/* Global variables */
long K, C, E, T, P, S;

/* Mutex to access log file */
pthread_mutex_t logAccess;

/* Mutex to access configuration file*/
pthread_mutex_t configAccess;



unsigned int convert(char *st);

/* Perform the difference between two timespec elements */
struct timespec diff(struct timespec start, struct timespec end);

struct timespec ms_to_timespec(unsigned int milliseconds);

struct timespec add_ts(struct timespec a, struct timespec b);

struct timespec mean_ts(struct timespec tot, unsigned int n);