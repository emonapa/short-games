#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define error_exit(err_code, fmt, ...) \
    error_exit_impl(__FILE__, __LINE__, err_code, fmt, ##__VA_ARGS__)

#define warning(fmt, ...) \
    warning_impl(__FILE__, __LINE__, fmt, ##__VA_ARGS__)


/* Types of errors */
typedef enum {
    ERR_NULL_POINTER = 1,
    ERR_SOLVE_WITH_0_MEM = 2,

    ERR_EMPTY_STACK = 50,          // Empty stack error

    ERR_MALLOC = 98,               // Memory allocation error
    ERR_OTHER = 99                 // Other unspecified errors
} ERROR_CODES;

/* Function to print an error message and terminate the program */
void error_exit_impl(const char* file, int line, ERROR_CODES err_code, const char *fmt, ...);

/* Function to print a warning message */
void warning_impl(const char* file, int line, const char *fmt, ...);



#endif  // ERROR_H
