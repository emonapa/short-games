/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include "parser.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"

typedef struct Parser {
    const Token *tokens;
    size_t index;
    LanguageError *error;
} Parser;

static const Token *current(const Parser *parser) {
    return &parser->tokens[parser->index];
}

static bool check(const Parser *parser, TokenType type) {
    return current(parser)->type == type;
}

static const Token *advance(Parser *parser) {
    const Token *token = current(parser);
    if (token->type != TOKEN_EOF) parser->index++;
    return token;
}

static bool parser_error_at(Parser *parser,
                            const Token *token,
                            const char *message) {
    language_error_set(parser->error,
                       token->offset,
                       token->line,
                       token->column,
                       "%s; got %s",
                       message,
                       token_type_name(token->type));
    return false;
}

static const Token *consume(Parser *parser,
                            TokenType type,
                            const char *message) {
    if (check(parser, type)) return advance(parser);
    parser_error_at(parser, current(parser), message);
    return NULL;
}

static AstNode *node_from_token(const Token *token) {
    return ast_node_new(token->start, token->length);
}

static AstNode *make_unary(const Token *operator_token, AstNode *child) {
    AstNode *node = node_from_token(operator_token);
    if (node == NULL) return NULL;

    if (!ast_node_add_child(node, child)) {
        free(node->text);
        free(node);
        return NULL;
    }

    return node;
}

static AstNode *make_binary(const Token *operator_token,
                            AstNode *left,
                            AstNode *right) {
    AstNode *node = node_from_token(operator_token);
    if (node == NULL) return NULL;

    if (!ast_node_add_child(node, left)) {
        free(node->text);
        free(node);
        return NULL;
    }

    if (!ast_node_add_child(node, right)) {
        /* node owns left after the first successful insertion. */
        ast_node_free(node);
        return NULL;
    }

    return node;
}

static bool begins_expression(TokenType type) {
    switch (type) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_LBRACE:
        case TOKEN_FUNC:
        case TOKEN_LPAREN:
        case TOKEN_INT:
        case TOKEN_STAR:
        case TOKEN_V:
        case TOKEN_DOWN:
        case TOKEN_CARET:
        case TOKEN_UP:
        case TOKEN_NUM_STAR:
        case TOKEN_NIMBER:
        case TOKEN_ARROW_STAR:
        case TOKEN_NUM_ARROW:
        case TOKEN_NUM_ARROW_STAR:
            return true;

        default:
            return false;
    }
}

static AstNode *parse_E(Parser *parser);

/* Parses the comma-separated expression list shared by O and A. */
static bool parse_expression_list(Parser *parser, AstNode *parent) {
    if (!begins_expression(current(parser)->type)) return true;

    AstNode *item = parse_E(parser);
    if (item == NULL) return false;

    if (!ast_node_add_child(parent, item)) {
        ast_node_free(item);
        language_error_set(parser->error, 0, 1, 1,
                           "memory allocation failed while building AST");
        return false;
    }

    while (check(parser, TOKEN_COMMA)) {
        advance(parser);

        item = parse_E(parser);
        if (item == NULL) return false;

        if (!ast_node_add_child(parent, item)) {
            ast_node_free(item);
            language_error_set(parser->error, 0, 1, 1,
                               "memory allocation failed while building AST");
            return false;
        }
    }

    return true;
}

/* P -> { O | O } */
static AstNode *parse_game_constructor(Parser *parser) {
    if (consume(parser, TOKEN_LBRACE, "expected '{'") == NULL) return NULL;

    AstNode *game = ast_node_new_cstr("{}");
    AstNode *left = ast_node_new_cstr("left");
    AstNode *right = ast_node_new_cstr("right");

    if (game == NULL || left == NULL || right == NULL) {
        ast_node_free(game);
        ast_node_free(left);
        ast_node_free(right);
        language_error_set(parser->error, 0, 1, 1,
                           "memory allocation failed while building AST");
        return NULL;
    }

    if (!parse_expression_list(parser, left)) goto fail;
    if (consume(parser, TOKEN_BAR, "expected '|' in game constructor") == NULL)
        goto fail;
    if (!parse_expression_list(parser, right)) goto fail;
    if (consume(parser, TOKEN_RBRACE, "expected '}' after game constructor") == NULL)
        goto fail;

    if (!ast_node_add_child(game, left)) goto memory_fail;
    left = NULL;
    if (!ast_node_add_child(game, right)) goto memory_fail;
    right = NULL;
    return game;

memory_fail:
    language_error_set(parser->error, 0, 1, 1,
                       "memory allocation failed while building AST");
fail:
    ast_node_free(game);
    ast_node_free(left);
    ast_node_free(right);
    return NULL;
}

/* P -> func ( A ) */
static AstNode *parse_function_call(Parser *parser) {
    const Token *name = consume(parser, TOKEN_FUNC, "expected function name");
    if (name == NULL) return NULL;

    AstNode *function = node_from_token(name);
    if (function == NULL) {
        language_error_set(parser->error, 0, 1, 1,
                           "memory allocation failed while building AST");
        return NULL;
    }

    if (consume(parser, TOKEN_LPAREN, "expected '(' after function name") == NULL)
        goto fail;
    if (!parse_expression_list(parser, function)) goto fail;
    if (consume(parser, TOKEN_RPAREN, "expected ')' after function arguments") == NULL)
        goto fail;

    return function;

fail:
    ast_node_free(function);
    return NULL;
}

/* P */
static AstNode *parse_P(Parser *parser) {
    const Token *token = current(parser);

    switch (token->type) {
        case TOKEN_LBRACE:
            return parse_game_constructor(parser);

        case TOKEN_FUNC:
            return parse_function_call(parser);

        case TOKEN_LPAREN: {
            advance(parser);
            AstNode *inside = parse_E(parser);
            if (inside == NULL) return NULL;

            if (consume(parser, TOKEN_RPAREN,
                        "expected ')' after parenthesized expression") == NULL) {
                ast_node_free(inside);
                return NULL;
            }
            return inside;
        }

        case TOKEN_INT:
        case TOKEN_STAR:
        case TOKEN_V:
        case TOKEN_DOWN:
        case TOKEN_CARET:
        case TOKEN_UP:
        case TOKEN_NUM_STAR:
        case TOKEN_NIMBER:
        case TOKEN_ARROW_STAR:
        case TOKEN_NUM_ARROW:
        case TOKEN_NUM_ARROW_STAR: {
            advance(parser);
            AstNode *literal = node_from_token(token);
            if (literal == NULL) {
                language_error_set(parser->error, token->offset,
                                   token->line, token->column,
                                   "memory allocation failed while building AST");
            }
            return literal;
        }

        default:
            parser_error_at(parser, token, "expected a primary expression");
            return NULL;
    }
}

/* U -> + U | - U | P */
static AstNode *parse_U(Parser *parser) {
    if (check(parser, TOKEN_PLUS) || check(parser, TOKEN_MINUS)) {
        const Token *operator_token = advance(parser);
        AstNode *child = parse_U(parser);
        if (child == NULL) return NULL;

        AstNode *node = make_unary(operator_token, child);
        if (node == NULL) {
            ast_node_free(child);
            language_error_set(parser->error,
                               operator_token->offset,
                               operator_token->line,
                               operator_token->column,
                               "memory allocation failed while building AST");
        }
        return node;
    }

    return parse_P(parser);
}

/* T -> U T', T' -> * U T' | / U T' | epsilon */
static AstNode *parse_T(Parser *parser) {
    AstNode *left = parse_U(parser);
    if (left == NULL) return NULL;

    while (check(parser, TOKEN_STAR) || check(parser, TOKEN_SLASH)) {
        const Token *operator_token = advance(parser);
        AstNode *right = parse_U(parser);
        if (right == NULL) {
            ast_node_free(left);
            return NULL;
        }

        AstNode *combined = make_binary(operator_token, left, right);
        if (combined == NULL) {
            ast_node_free(left);
            ast_node_free(right);
            language_error_set(parser->error,
                               operator_token->offset,
                               operator_token->line,
                               operator_token->column,
                               "memory allocation failed while building AST");
            return NULL;
        }
        left = combined;
    }

    return left;
}

/* E -> T E', E' -> + T E' | - T E' | epsilon */
static AstNode *parse_E(Parser *parser) {
    AstNode *left = parse_T(parser);
    if (left == NULL) return NULL;

    while (check(parser, TOKEN_PLUS) || check(parser, TOKEN_MINUS)) {
        const Token *operator_token = advance(parser);
        AstNode *right = parse_T(parser);
        if (right == NULL) {
            ast_node_free(left);
            return NULL;
        }

        AstNode *combined = make_binary(operator_token, left, right);
        if (combined == NULL) {
            ast_node_free(left);
            ast_node_free(right);
            language_error_set(parser->error,
                               operator_token->offset,
                               operator_token->line,
                               operator_token->column,
                               "memory allocation failed while building AST");
            return NULL;
        }
        left = combined;
    }

    return left;
}

AstNode *parser_parse(const Token *tokens, LanguageError *error) {
    language_error_clear(error);

    if (tokens == NULL) {
        language_error_set(error, 0, 1, 1, "tokens is NULL");
        return NULL;
    }

    Parser parser = {
        .tokens = tokens,
        .index = 0,
        .error = error
    };

    AstNode *root = parse_E(&parser);
    if (root == NULL) return NULL;

    if (!check(&parser, TOKEN_EOF)) {
        parser_error_at(&parser, current(&parser),
                        "unexpected token after complete expression");
        ast_node_free(root);
        return NULL;
    }

    return root;
}

AstNode *parser_parse_text(const char *source, LanguageError *error) {
    Token *tokens = NULL;
    if (!lexer_tokenize(source, &tokens, error)) return NULL;

    AstNode *root = parser_parse(tokens, error);
    lexer_tokens_free(&tokens);
    return root;
}
