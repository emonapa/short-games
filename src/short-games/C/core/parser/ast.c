/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include "ast.h"

#include <stdlib.h>
#include <string.h>

#include "../../shared/darray.h"

AstNode *ast_node_new(const char *text, size_t length) {
    if (text == NULL) return NULL;

    AstNode *node = malloc(sizeof(*node));
    if (node == NULL) return NULL;

    node->text = malloc(length + 1);
    if (node->text == NULL) {
        free(node);
        return NULL;
    }

    memcpy(node->text, text, length);
    node->text[length] = '\0';
    node->children = NULL;
    return node;
}

AstNode *ast_node_new_cstr(const char *text) {
    return text == NULL ? NULL : ast_node_new(text, strlen(text));
}

bool ast_node_add_child(AstNode *parent, AstNode *child) {
    if (parent == NULL || child == NULL) return false;

    size_t old_length = da_len(parent->children);
    da_push(parent->children, child);
    return parent->children != NULL &&
           da_len(parent->children) == old_length + 1;
}

size_t ast_node_child_count(const AstNode *node) {
    return node == NULL ? 0 : da_len(node->children);
}

void ast_node_free(AstNode *node) {
    if (node == NULL) return;

    for (size_t i = 0; i < da_len(node->children); i++) {
        ast_node_free(node->children[i]);
    }

    da_free(node->children);
    free(node->text);
    free(node);
}

void ast_node_print(const AstNode *node, FILE *stream, size_t indent) {
    if (node == NULL || stream == NULL) return;

    fprintf(stream, "%*s%s\n", (int)indent, "", node->text);
    for (size_t i = 0; i < da_len(node->children); i++) {
        ast_node_print(node->children[i], stream, indent + 2);
    }
}
