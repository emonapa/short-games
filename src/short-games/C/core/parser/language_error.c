/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include "language_error.h"

#include <stdarg.h>
#include <stdio.h>

void language_error_clear(LanguageError *error) {
    if (error == NULL) return;

    error->offset = 0;
    error->line = 1;
    error->column = 1;
    error->message[0] = '\0';
}

void language_error_set(LanguageError *error,
                        size_t offset,
                        size_t line,
                        size_t column,
                        const char *format,
                        ...) {
    if (error == NULL) return;

    error->offset = offset;
    error->line = line;
    error->column = column;

    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}
