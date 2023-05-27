#include <pthread.h>
#include <stdbool.h>

#pragma once

/* Global variables */
long K, C, E, T, P, S;

/* Mutex to access log file */
pthread_mutex_t logAccess;

/* Mutex to access configuration file*/
pthread_mutex_t configAccess;

unsigned int convert(char *st);
