/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>

typedef enum TokenType {
    TOKEN_EOF = 0,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,

    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_COMMA,
    TOKEN_BAR,

    TOKEN_INT,
    TOKEN_FUNC,

    TOKEN_V,
    TOKEN_DOWN,
    TOKEN_CARET,
    TOKEN_UP,

    TOKEN_NUM_STAR,
    TOKEN_NIMBER,
    TOKEN_ARROW_STAR,

    /* Existing project notation supported in addition to the supplied
     * grammar: 2^, 2v, 2↑, 2↓ and the variants ending in '*'. */
    TOKEN_NUM_ARROW,
    TOKEN_NUM_ARROW_STAR
} TokenType;

typedef struct Token {
    TokenType type;
    const char *start;
    size_t length;
    size_t offset;
    size_t line;
    size_t column;
} Token;

const char *token_type_name(TokenType type);

#endif // TOKEN_H
