#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h>

#ifndef __GLOB_H
#define __GLOB_H

#define MAX_LINE 50
#define LOG_FILENAME "log.txt" // TAKE LOG FILENAME FROM CONFIG FILE
#define CONFIG_FILENAME "lib/config.txt"
#define SUPMRKT_EXEC_PATH "supermarket"
#define SOCKET_FILENAME "/tmp/9Lq7BNBnBycd6nxy.socket"

extern unsigned int K, C, E, T, P, S;

/* Mutex to access configuration file*/
extern pthread_mutex_t configAccess;

/* Perform the difference between two timespec elements. Return
   a timespec struct with the result. */
struct timespec diff(struct timespec start, struct timespec end);

/* Convert milliseconds into timespec data structure. Return 
   the converted data. */
struct timespec ms_to_timespec(unsigned int milliseconds);

/* Perform the sum between two timespec elements. Return
   a timespec struct with the result. */
struct timespec add_ts(struct timespec a, struct timespec b);

/* Convert time in timespec format into milliseconds. Return an
   unsigned int. */
unsigned int timespec_to_ms(struct timespec ts);

#endif