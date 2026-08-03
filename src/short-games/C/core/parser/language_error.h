/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef LANGUAGE_ERROR_H
#define LANGUAGE_ERROR_H

#include <stddef.h>

typedef struct LanguageError {
    size_t offset;
    size_t line;
    size_t column;
    char message[256];
} LanguageError;

void language_error_clear(LanguageError *error);
void language_error_set(LanguageError *error,
                        size_t offset,
                        size_t line,
                        size_t column,
                        const char *format,
                        ...);

#endif // LANGUAGE_ERROR_H
