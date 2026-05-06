/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hackenbushe s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hackenbush Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define error_exit(err_code, fmt, ...) \
    error_exit_impl(__FILE__, __LINE__, err_code, fmt, ##__VA_ARGS__)

#define warning(fmt, ...) \
    warning_impl(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

// Types of errors
typedef enum {
    ERR_NULL_POINTER = -1,
    ERR_SOLVE_WITH_NONPOSITIVE_MEM = -2, // trying to malloc with size <= 0

    ERR_EMPTY_STACK = -50,

    ERR_MALLOC = -98,               // Memory allocation error
    ERR_OTHER = -99                 // Other unspecified errors
} ERROR_CODES;

// Function to print a warning message
static inline void warning_impl(const char* file, int line, const char *fmt, ...) {
    va_list args;

    fprintf(stderr, "%s:%d warning: ", file, line);

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

// Function to print an error message and terminate the program
static inline void error_exit_impl(const char* file, int line, ERROR_CODES err_code, const char *fmt, ...) {
    va_list args;

    fprintf(stderr, "%s:%d error: ", file, line);

    switch (err_code) {
        case ERR_MALLOC:
            fprintf(stderr, "Malloc failed.\n");
            break;

        case ERR_NULL_POINTER:
            fprintf(stderr, "Trying to dereference null pointer.\n");
            break;

        case ERR_SOLVE_WITH_NONPOSITIVE_MEM:
            fprintf(stderr, "Trying to allocate memory with non-positive size.\n");
            break;

        case ERR_EMPTY_STACK:
            fprintf(stderr, "Trying to access an empty stack.\n");
            break;

        case ERR_OTHER:
        default:
            fprintf(stderr, "Something went wrong.\n");
            break;
    }

    if (!(fmt == NULL || fmt[0] == '\0')) {
        fprintf(stderr, "[ADDITIONAL] ");

        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }

    exit(err_code);
}

#endif  // ERROR_H
