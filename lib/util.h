#if !defined(UTIL_H)
#define UTIL_H

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#if !defined(BUFSIZE)
#define BUFSIZE 256
#endif

#define USEC_TO_MSEC(val_to_conv)   \
    {                               \
        return val_to_conv * 0.001; \
    }
#define MSEC_TO_NSEC(val_to_conv) ((val_to_conv % 1000) * 1000000)

#define CHECK_NEQ(x, val, str)  \
    if ((x) != val)             \
    {                           \
        perror(#str);           \
        int errno_copy = errno; \
        exit(errno_copy);       \
    }

#define CHECK_EQ(x, val, str)   \
    if ((x) == val)             \
    {                           \
        perror(#str);           \
        int errno_copy = errno; \
        exit(errno_copy);       \
    }

#endif 