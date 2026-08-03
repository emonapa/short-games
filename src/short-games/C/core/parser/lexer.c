/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include "lexer.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "../../shared/darray.h"

#define UTF8_UP   "\xE2\x86\x91"
#define UTF8_DOWN "\xE2\x86\x93"

typedef struct Lexer {
    const char *current;
    size_t offset;
    size_t line;
    size_t column;
    bool expect_primary;
    Token *tokens;
    LanguageError *error;
} Lexer;

static bool starts_with(const char *text, const char *prefix) {
    size_t length = strlen(prefix);
    return strncmp(text, prefix, length) == 0;
}

static bool is_identifier_start(unsigned char c) {
    return isalpha(c) || c == '_';
}

static bool is_identifier_continue(unsigned char c) {
    return isalnum(c) || c == '_';
}

static bool is_literal_terminator(unsigned char c) {
    return c == '\0' || isspace(c) || c == '+' || c == '-' ||
           c == '/' || c == ')' || c == '}' || c == ',' || c == '|';
}

static void lexer_advance_ascii(Lexer *lexer) {
    if (*lexer->current == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }

    lexer->current++;
    lexer->offset++;
}

static void lexer_advance_symbol(Lexer *lexer, size_t byte_count) {
    lexer->current += byte_count;
    lexer->offset += byte_count;
    lexer->column++;
}

static bool lexer_push(Lexer *lexer,
                       TokenType type,
                       const char *start,
                       size_t length,
                       size_t offset,
                       size_t line,
                       size_t column) {
    Token token = {
        .type = type,
        .start = start,
        .length = length,
        .offset = offset,
        .line = line,
        .column = column
    };

    size_t old_length = da_len(lexer->tokens);
    da_push(lexer->tokens, token);
    if (lexer->tokens == NULL || da_len(lexer->tokens) != old_length + 1) {
        language_error_set(lexer->error, offset, line, column,
                           "memory allocation failed while storing tokens");
        return false;
    }

    switch (type) {
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
        case TOKEN_RPAREN:
        case TOKEN_RBRACE:
            lexer->expect_primary = false;
            break;

        case TOKEN_EOF:
            break;

        default:
            lexer->expect_primary = true;
            break;
    }

    return true;
}

static void lexer_skip_whitespace(Lexer *lexer) {
    while (isspace((unsigned char)*lexer->current)) {
        lexer_advance_ascii(lexer);
    }
}

static size_t arrow_length_at(const char *text, TokenType *plain_type) {
    if (*text == '^') {
        if (plain_type != NULL) *plain_type = TOKEN_CARET;
        return 1;
    }

    if (*text == 'v') {
        if (plain_type != NULL) *plain_type = TOKEN_V;
        return 1;
    }

    if (starts_with(text, UTF8_UP)) {
        if (plain_type != NULL) *plain_type = TOKEN_UP;
        return 3;
    }

    if (starts_with(text, UTF8_DOWN)) {
        if (plain_type != NULL) *plain_type = TOKEN_DOWN;
        return 3;
    }

    return 0;
}

static bool lexer_number(Lexer *lexer) {
    const char *start = lexer->current;
    size_t offset = lexer->offset;
    size_t line = lexer->line;
    size_t column = lexer->column;

    while (isdigit((unsigned char)*lexer->current)) {
        lexer_advance_ascii(lexer);
    }

    size_t arrow_len = arrow_length_at(lexer->current, NULL);
    if (arrow_len != 0) {
        lexer_advance_symbol(lexer, arrow_len);
        TokenType type = TOKEN_NUM_ARROW;

        if (*lexer->current == '*') {
            lexer_advance_ascii(lexer);
            type = TOKEN_NUM_ARROW_STAR;
        }

        return lexer_push(lexer, type, start,
                          (size_t)(lexer->current - start),
                          offset, line, column);
    }

    /* n* is an atomic number-plus-star literal only when the star terminates
     * the primary. Consequently 2*3 remains INT STAR INT. */
    if (*lexer->current == '*' &&
        is_literal_terminator((unsigned char)lexer->current[1])) {
        lexer_advance_ascii(lexer);
        return lexer_push(lexer, TOKEN_NUM_STAR, start,
                          (size_t)(lexer->current - start),
                          offset, line, column);
    }

    return lexer_push(lexer, TOKEN_INT, start,
                      (size_t)(lexer->current - start),
                      offset, line, column);
}

static bool lexer_identifier(Lexer *lexer) {
    const char *start = lexer->current;
    size_t offset = lexer->offset;
    size_t line = lexer->line;
    size_t column = lexer->column;

    while (is_identifier_continue((unsigned char)*lexer->current)) {
        lexer_advance_ascii(lexer);
    }

    const char *lookahead = lexer->current;
    while (isspace((unsigned char)*lookahead)) lookahead++;

    if (*lookahead != '(') {
        language_error_set(lexer->error, offset, line, column,
                           "identifier '%.*s' must be followed by '('",
                           (int)(lexer->current - start), start);
        return false;
    }

    return lexer_push(lexer, TOKEN_FUNC, start,
                      (size_t)(lexer->current - start),
                      offset, line, column);
}

static bool lexer_arrow(Lexer *lexer) {
    const char *start = lexer->current;
    size_t offset = lexer->offset;
    size_t line = lexer->line;
    size_t column = lexer->column;
    TokenType plain_type;
    size_t arrow_len = arrow_length_at(lexer->current, &plain_type);

    lexer_advance_symbol(lexer, arrow_len);

    if (*lexer->current == '*') {
        lexer_advance_ascii(lexer);
        return lexer_push(lexer, TOKEN_ARROW_STAR, start,
                          (size_t)(lexer->current - start),
                          offset, line, column);
    }

    return lexer_push(lexer, plain_type, start, arrow_len,
                      offset, line, column);
}

bool lexer_tokenize(const char *source, Token **out_tokens, LanguageError *error) {
    if (out_tokens == NULL) {
        language_error_set(error, 0, 1, 1, "out_tokens is NULL");
        return false;
    }

    *out_tokens = NULL;
    language_error_clear(error);

    if (source == NULL) {
        language_error_set(error, 0, 1, 1, "source is NULL");
        return false;
    }

    Lexer lexer = {
        .current = source,
        .offset = 0,
        .line = 1,
        .column = 1,
        .expect_primary = true,
        .tokens = NULL,
        .error = error
    };

    while (true) {
        lexer_skip_whitespace(&lexer);

        const char *start = lexer.current;
        size_t offset = lexer.offset;
        size_t line = lexer.line;
        size_t column = lexer.column;
        unsigned char c = (unsigned char)*lexer.current;

        if (c == '\0') {
            if (!lexer_push(&lexer, TOKEN_EOF, start, 0,
                            offset, line, column)) {
                lexer_tokens_free(&lexer.tokens);
                return false;
            }
            break;
        }

        if (isdigit(c)) {
            if (!lexer_number(&lexer)) {
                lexer_tokens_free(&lexer.tokens);
                return false;
            }
            continue;
        }

        if (is_identifier_start(c) &&
            !(c == 'v' && !is_identifier_continue((unsigned char)lexer.current[1]))) {
            if (!lexer_identifier(&lexer)) {
                lexer_tokens_free(&lexer.tokens);
                return false;
            }
            continue;
        }

        if (arrow_length_at(lexer.current, NULL) != 0) {
            if (!lexer_arrow(&lexer)) {
                lexer_tokens_free(&lexer.tokens);
                return false;
            }
            continue;
        }

        lexer_advance_ascii(&lexer);

        TokenType type;
        switch (c) {
            case '+': type = TOKEN_PLUS;   break;
            case '-': type = TOKEN_MINUS;  break;
            case '/': type = TOKEN_SLASH;  break;
            case '(': type = TOKEN_LPAREN; break;
            case ')': type = TOKEN_RPAREN; break;
            case '{': type = TOKEN_LBRACE; break;
            case '}': type = TOKEN_RBRACE; break;
            case ',': type = TOKEN_COMMA;  break;
            case '|': type = TOKEN_BAR;    break;

            case '*': {
                if (lexer.expect_primary &&
                    isdigit((unsigned char)*lexer.current)) {
                    while (isdigit((unsigned char)*lexer.current)) {
                        lexer_advance_ascii(&lexer);
                    }
                    type = TOKEN_NIMBER;
                } else {
                    type = TOKEN_STAR;
                }
                break;
            }

            default:
                language_error_set(error, offset, line, column,
                                   "unexpected byte 0x%02X", c);
                lexer_tokens_free(&lexer.tokens);
                return false;
        }

        if (!lexer_push(&lexer, type, start,
                        (size_t)(lexer.current - start),
                        offset, line, column)) {
            lexer_tokens_free(&lexer.tokens);
            return false;
        }
    }

    *out_tokens = lexer.tokens;
    return true;
}

void lexer_tokens_free(Token **tokens) {
    if (tokens == NULL) return;
    da_free(*tokens);
}
