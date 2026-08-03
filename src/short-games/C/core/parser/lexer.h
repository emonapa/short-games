/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>

#include "language_error.h"
#include "token.h"

/**
 * @brief Tokenizes a null-terminated UTF-8 expression.
 *
 * Produces a stretchy-buffer array of Token values.
 * The token text points into source, so source must remain alive while the
 * tokens are used. Release the array with lexer_tokens_free().
 */
bool lexer_tokenize(const char *source, Token **out_tokens, LanguageError *error);
void lexer_tokens_free(Token **tokens);

#endif // LEXER_H
