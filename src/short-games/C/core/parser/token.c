/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include "token.h"

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF:            return "end of input";
        case TOKEN_PLUS:           return "+";
        case TOKEN_MINUS:          return "-";
        case TOKEN_STAR:           return "*";
        case TOKEN_SLASH:          return "/";
        case TOKEN_LPAREN:         return "(";
        case TOKEN_RPAREN:         return ")";
        case TOKEN_LBRACE:         return "{";
        case TOKEN_RBRACE:         return "}";
        case TOKEN_COMMA:          return ",";
        case TOKEN_BAR:            return "|";
        case TOKEN_INT:            return "integer";
        case TOKEN_FUNC:           return "function";
        case TOKEN_V:              return "v";
        case TOKEN_DOWN:           return "↓";
        case TOKEN_CARET:          return "^";
        case TOKEN_UP:             return "↑";
        case TOKEN_NUM_STAR:       return "number-star";
        case TOKEN_NIMBER:         return "nimber";
        case TOKEN_ARROW_STAR:     return "arrow-star";
        case TOKEN_NUM_ARROW:      return "number-arrow";
        case TOKEN_NUM_ARROW_STAR: return "number-arrow-star";
        default:                   return "unknown token";
    }
}
