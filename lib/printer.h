enum actor_id 
{
    DIRECTOR,
    SUPERMARKET,
    CASHIER,
    CUSTOMER
};

void mod_printf(enum actor_id, char* str_to_screen);
void mod_perror(enum actor_id, char* error);

#ifndef H_DIR_PRINTER
    #define DIR_PRINTF(s) mod_printf(0, s)
    #define DIR_PERROR(s) mod_perror(0, s)
#endif

#ifndef H_SUPMRKT_PRINTER
    #define SUPMRKT_PRINTF(s) mod_printf(1, s)
    #define SUPMRKT_PERROR(s) mod_perror(1, s)
#endif

#ifndef H_CASHIER_PRINTER
    #define CASHIER_PRINTF(s) mod_printf(2, s)
    #define CASHIER_PERROR(s) mod_perror(2, s)
#endif

#ifndef H_CUSTOMER_PRINTER
    #define CUSTOMER_PRINTF(s) mod_printf(3, s)
    #define CUSTOMER_PERROR(s) mod_perror(3, s)
#endif

