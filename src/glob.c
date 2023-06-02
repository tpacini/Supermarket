#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "glob.h"

unsigned int convert(char *st)
{
    char *x;
    for (x = st; *x; x++)
    {
        if (!isdigit(*x))
            return NULL;
    }
    return (strtoul(st, NULL, 10));
}

// SAFE???
struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;

    if ((end.tv_nsec - start.tv_nsec) < 0)
    {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    }
    else
    {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}

// CORRECT; SAFE???
struct timespec ms_to_timespec(unsigned int milliseconds)
{
    struct timespec t;

    t.tv_nsec = (milliseconds % 1000) * 1000000;
    t.tv_sec = milliseconds / 1000;

    return t;
}