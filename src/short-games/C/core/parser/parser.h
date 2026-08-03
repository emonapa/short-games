/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "language_error.h"
#include "token.h"

/** @brief Parses a token array produced by lexer_tokenize(). */
AstNode *parser_parse(const Token *tokens, LanguageError *error);

/** @brief Runs lexical and syntactic analysis in one call. */
AstNode *parser_parse_text(const char *source, LanguageError *error);

#endif // PARSER_H
