#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "glob.h"

unsigned int convert(char *st)
{
    char *x;
    for (x = st; *x; x++)
    {
        if (!isdigit(*x))
            return UINT_MAX;
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

struct timespec ms_to_timespec(unsigned int milliseconds)
{
    struct timespec t;
    int microsec = 1000000;

    /* nanoseconds = milliseconds * microseconds */
    t.tv_nsec = (milliseconds % 1000) * microsec;
    
    t.tv_sec = milliseconds / 1000;

    return t;
}

struct timespec add_ts(struct timespec a, struct timespec b)
{
    struct timespec t;
    unsigned int temp;
    unsigned int one_sec_ns = 1000000000;
    int offset = 0;

    /* If we reach more than one seconds in ns, we add 
       those seconds to t.tv_sec */
    temp = a.tv_nsec + b.tv_nsec;
    if (temp > one_sec_ns){
        offset = temp / one_sec_ns;
        t.tv_nsec = temp % one_sec_ns;
    }
    else
        t.tv_nsec = temp;

    t.tv_sec = a.tv_sec + b.tv_sec + offset;

    return t;
}