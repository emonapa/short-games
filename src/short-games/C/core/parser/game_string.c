/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include "game_string.h"

#include <stdio.h>

#include "ast.h"
#include "language_error.h"
#include "parser.h"
#include "semantic.h"

static char last_error[384];

static void store_error(const LanguageError *error) {
    if (error == NULL || error->message[0] == '\0') {
        snprintf(last_error, sizeof(last_error), "unknown parser error");
        return;
    }

    snprintf(last_error, sizeof(last_error),
             "line %zu, column %zu: %s",
             error->line, error->column, error->message);
}

Game *game_from_string(const char *text) {
    last_error[0] = '\0';

    LanguageError error;
    AstNode *root = parser_parse_text(text, &error);
    if (root == NULL) {
        store_error(&error);
        return NULL;
    }

    Game *result = semantic_evaluate(root, &error);
    ast_node_free(root);

    if (result == NULL) {
        store_error(&error);
        return NULL;
    }

    return result;
}

const char *game_string_last_error(void) {
    return last_error;
}
