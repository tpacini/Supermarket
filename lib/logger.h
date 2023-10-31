#define DEBUG 1

enum log_level 
{
    FATAL,
    ERROR,
    DEBUG
};

void log_output(enum log_level, char* label, char* message);

#ifdef DEBUG
    #define DIR_LOG_DEBUG(s)        log_output(2, "[DIR] ", s)
    #define SUPMRKT_LOG_DEBUG(s)    log_output(2, "[SUPMRKT] ", s)
    #define CASHIER_LOG_DEBUG(s)    log_output(2, "[CASHIER] ", s)
    #define CUSTOMER_LOG_DEBUG(s)   log_output(2, "[CUSTOMER] ", s)
#else
    #define DIR_LOG_DEBUG(s)
    #define SUPMRKT_LOG_DEBUG(s)
    #define CASHIER_LOG_DEBUG(s)
    #define CUSTOMER_LOG_DEBUG(s)
#endif

#ifndef H_DIR_LOGGER
    #define DIR_LOG_FATAL(s) log_output(0, "[DIR] ", s)
    #define DIR_LOG_ERROR(s) log_output(1, "[DIR] ", s)
#endif

#ifndef H_SUPMRKT_LOGGER
    #define SUPMRKT_LOG_FATAL(s) log_output(0, "[SUPMRKT] ", s)
    #define SUPMRKT_LOG_ERROR(s) log_output(1, "[SUPMRKT] ", s)
#endif

#ifndef H_CASHIER_LOGGER
    #define CASHIER_LOG_FATAL(s) log_output(0, "[CASHIER] ", s)
    #define CASHIER_LOG_ERROR(s) log_output(1, "[CASHIER] ", s)
#endif

#ifndef H_CUSTOMER_LOGGER
    #define CUSTOMER_LOG_FATAL(s) log_output(0, "[CUSTOMER] ", s)
    #define CUSTOMER_LOG_ERROR(s) log_output(1, "[CUSTOMER] ", s)
#endif

