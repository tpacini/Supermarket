#include <stdio.h>
#include "logger.h"

void log_output(enum log_level id, char *label, char *message)
{
    char* prefix[3] = { "[FATAL]",
                        "[ERROR]",
                        "[DEBUG]"};

    fprintf(stdout, "%s%s%s\n", prefix[id], label, message);
}