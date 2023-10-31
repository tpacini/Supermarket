#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

#include "glob.h"

// TODO: check the entire file

struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;

    if ((end.tv_nsec - start.tv_nsec) < 0)
    {
        temp.tv_sec = end.tv_sec - start.tv_sec;
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

unsigned int timespec_to_ms(struct timespec ts)
{
    unsigned int ms = 0;
    int nanosec = 1000000000;

    ms = (ts.tv_sec * 1000) + (round(ts.tv_nsec / nanosec) * 1000);

    return ms;
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