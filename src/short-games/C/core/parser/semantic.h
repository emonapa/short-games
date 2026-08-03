/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdbool.h>

#include "ast.h"
#include "language_error.h"
#include "../short_game.h"

/** @brief Checks AST shape, literal forms, function names and arity. */
bool semantic_validate(const AstNode *root, LanguageError *error);

/** @brief Validates and evaluates an AST to a canonical game. */
Game *semantic_evaluate(const AstNode *root, LanguageError *error);

#endif // SEMANTIC_H
