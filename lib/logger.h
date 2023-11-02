#include <stdio.h>

#define IS_DEBUG 1

#ifdef IS_DEBUG
#define LOG_DEBUG(s)                                                  \
    {                                                                 \
        fprintf(stdout, "[DEBUG][%s,%d]: %s", __FILE__, __LINE__, s); \
    }
#else
#define LOG_DEBUG(s)
#endif

#define LOG_FATAL(s)                                                  \
    {                                                                 \
        fprintf(stdout, "[FATAL][%s,%d]: %s", __FILE__, __LINE__, s); \
    }

#define LOG_ERROR(s)                                                  \
    {                                                                 \
        fprintf(stdout, "[ERROR][%s,%d]: %s", __FILE__, __LINE__, s); \
    }

#define MOD_PERROR(error)                                        \
    {                                                            \
        fprintf(stdout, "[PERROR][%s, %d]", __FILE__, __LINE__); \
        perror(error);                                           \
    }

#define MOD_PRINTF(print_str)                               \
    {                                                       \
        fprintf(stdout, "[%s]: %s\n", __FILE__, print_str); \
    }