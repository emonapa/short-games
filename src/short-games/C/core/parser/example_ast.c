/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include <stdio.h>

#include "ast.h"
#include "language_error.h"
#include "parser.h"

int main(void) {
    const char *source =
        "canonical({0, *2 | ↑*, cool({ | 0})}) + 2 * (3 - 1)";

    LanguageError error;
    AstNode *root = parser_parse_text(source, &error);
    if (root == NULL) {
        fprintf(stderr, "line %zu, column %zu: %s\n",
                error.line, error.column, error.message);
        return 1;
    }

    ast_node_print(root, stdout, 0);
    ast_node_free(root);
    return 0;
}
