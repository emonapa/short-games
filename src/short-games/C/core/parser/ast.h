/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/** @brief Expression tree shared by the parser and evaluator. */
typedef struct AstNode {
    char *text;
    struct AstNode **children;
} AstNode;

AstNode *ast_node_new(const char *text, size_t length);
AstNode *ast_node_new_cstr(const char *text);
bool ast_node_add_child(AstNode *parent, AstNode *child);
size_t ast_node_child_count(const AstNode *node);
void ast_node_free(AstNode *node);

/** @brief Prints an AST as an indented tree for diagnostics and examples. */
void ast_node_print(const AstNode *node, FILE *stream, size_t indent);

#endif // AST_H
