#include "error.h"

/* Function to print a warning message */
void warning_impl(const char* file, int line, const char *fmt, ...) {
    va_list args;

    // Vypíše predponu pre varovanie
    fprintf(stderr, "%s:%d warning: ", file, line);

    // Formátovaný výpis varovania
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

/* Function to print an error message and terminate the program */
void error_exit_impl(const char* file, int line, ERROR_CODES err_code, const char *fmt, ...) {
    va_list args;

    fprintf(stderr, "%s:%d error: ", file, line);

    /* Formatted error message */
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    /* Terminate the program with the error code */
    exit(err_code);
}
