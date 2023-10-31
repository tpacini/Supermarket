#include <stdio.h>
#include "printer.h"

void mod_printf(enum actor_id id, char* str_to_screen)
{
    char* prefix[4] = {"[DIR] ", 
                       "[SUPMRKT] ", 
                       "[CASHIER] ", 
                       "[CUSTOMER] "};

    fprintf(stdout, "%s%s\n", prefix[id], str_to_screen);
}

void mod_perror(enum actor_id id, char* error)
{
    char *prefix[4] = { "[DIR] ",
                        "[SUPMRKT] ",
                        "[CASHIER] ",
                        "[CUSTOMER] " };

    fprintf(stdout, "[PERROR]%s", prefix[id]);
    perror(error); // TODO: check if the output is readable and makes sense
}