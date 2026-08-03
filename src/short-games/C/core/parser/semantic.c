/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include "semantic.h"

#include <ctype.h>
#include <limits.h>
#include <string.h>

#include "../../shared/darray.h"
#include "dyadic_value.h"
#include "../game_darray.h"
#include "../singletons.h"

#define UTF8_UP   "\xE2\x86\x91"
#define UTF8_DOWN "\xE2\x86\x93"

typedef enum LiteralKind {
    LITERAL_INVALID = 0,
    LITERAL_INT,
    LITERAL_STAR,
    LITERAL_UP,
    LITERAL_DOWN,
    LITERAL_NUM_STAR,
    LITERAL_NIMBER,
    LITERAL_NUM_UP,
    LITERAL_NUM_DOWN,
    LITERAL_UP_STAR,
    LITERAL_DOWN_STAR,
    LITERAL_NUM_UP_STAR,
    LITERAL_NUM_DOWN_STAR
} LiteralKind;

typedef struct LiteralInfo {
    LiteralKind kind;
    int number;
} LiteralInfo;

static bool text_is(const AstNode *node, const char *text) {
    return node != NULL && strcmp(node->text, text) == 0;
}

static bool parse_nonnegative_int(const char *begin,
                                  const char *end,
                                  int *out_value) {
    if (begin == NULL || end == NULL || begin >= end) return false;

    int value = 0;
    for (const char *current = begin; current < end; current++) {
        if (!isdigit((unsigned char)*current)) return false;
        int digit = *current - '0';
        if (value > (INT_MAX - digit) / 10) return false;
        value = value * 10 + digit;
    }

    if (out_value != NULL) *out_value = value;
    return true;
}

static bool suffix_is_up(const char *suffix) {
    return strcmp(suffix, "^") == 0 || strcmp(suffix, UTF8_UP) == 0;
}

static bool suffix_is_down(const char *suffix) {
    return strcmp(suffix, "v") == 0 || strcmp(suffix, UTF8_DOWN) == 0;
}

static LiteralInfo classify_literal(const char *text) {
    LiteralInfo info = { .kind = LITERAL_INVALID, .number = 0 };
    if (text == NULL || text[0] == '\0') return info;

    if (strcmp(text, "*") == 0) {
        info.kind = LITERAL_STAR;
        return info;
    }
    if (suffix_is_up(text)) {
        info.kind = LITERAL_UP;
        info.number = 1;
        return info;
    }
    if (suffix_is_down(text)) {
        info.kind = LITERAL_DOWN;
        info.number = 1;
        return info;
    }
    if (strcmp(text, "^*") == 0 || strcmp(text, UTF8_UP "*") == 0) {
        info.kind = LITERAL_UP_STAR;
        info.number = 1;
        return info;
    }
    if (strcmp(text, "v*") == 0 || strcmp(text, UTF8_DOWN "*") == 0) {
        info.kind = LITERAL_DOWN_STAR;
        info.number = 1;
        return info;
    }

    size_t length = strlen(text);
    if (text[0] == '*' && length > 1) {
        if (parse_nonnegative_int(text + 1, text + length, &info.number)) {
            info.kind = LITERAL_NIMBER;
        }
        return info;
    }

    const char *digits_end = text;
    while (isdigit((unsigned char)*digits_end)) digits_end++;
    if (digits_end == text ||
        !parse_nonnegative_int(text, digits_end, &info.number)) {
        return info;
    }

    const char *suffix = digits_end;
    if (*suffix == '\0') {
        info.kind = LITERAL_INT;
    } else if (strcmp(suffix, "*") == 0) {
        info.kind = LITERAL_NUM_STAR;
    } else if (suffix_is_up(suffix)) {
        info.kind = LITERAL_NUM_UP;
    } else if (suffix_is_down(suffix)) {
        info.kind = LITERAL_NUM_DOWN;
    } else if (strcmp(suffix, "^*") == 0 ||
               strcmp(suffix, UTF8_UP "*") == 0) {
        info.kind = LITERAL_NUM_UP_STAR;
    } else if (strcmp(suffix, "v*") == 0 ||
               strcmp(suffix, UTF8_DOWN "*") == 0) {
        info.kind = LITERAL_NUM_DOWN_STAR;
    }
    return info;
}

static bool is_known_function(const char *name) {
    return strcmp(name, "canonical") == 0 ||
           strcmp(name, "canonicalize") == 0 ||
           strcmp(name, "cool") == 0 ||
           strcmp(name, "projection") == 0 ||
           strcmp(name, "star_projection") == 0;
}

static bool semantic_validate_node(const AstNode *node, LanguageError *error);

static bool validate_children(const AstNode *node, LanguageError *error) {
    for (size_t i = 0; i < ast_node_child_count(node); i++) {
        if (!semantic_validate_node(node->children[i], error)) return false;
    }
    return true;
}

static bool semantic_validate_node(const AstNode *node, LanguageError *error) {
    if (node == NULL || node->text == NULL) {
        language_error_set(error, 0, 1, 1, "AST contains a NULL node");
        return false;
    }

    size_t count = ast_node_child_count(node);
    if (text_is(node, "{}")) {
        if (count != 2 || !text_is(node->children[0], "left") ||
            !text_is(node->children[1], "right")) {
            language_error_set(error, 0, 1, 1,
                               "game constructor must contain left and right lists");
            return false;
        }
        return validate_children(node->children[0], error) &&
               validate_children(node->children[1], error);
    }

    if (text_is(node, "left") || text_is(node, "right")) {
        language_error_set(error, 0, 1, 1,
                           "option-list node appears outside a game constructor");
        return false;
    }

    if (text_is(node, "+") || text_is(node, "-")) {
        if (count != 1 && count != 2) {
            language_error_set(error, 0, 1, 1,
                               "operator '%s' expects one or two operands",
                               node->text);
            return false;
        }
        return validate_children(node, error);
    }

    if ((text_is(node, "*") && count != 0) || text_is(node, "/")) {
        if (count != 2) {
            language_error_set(error, 0, 1, 1,
                               "%s expects two operands",
                               text_is(node, "*") ? "multiplication" : "division");
            return false;
        }
        return validate_children(node, error);
    }

    if (is_known_function(node->text)) {
        if (count != 1) {
            language_error_set(error, 0, 1, 1,
                               "function '%s' expects exactly one argument",
                               node->text);
            return false;
        }
        return validate_children(node, error);
    }

    if (count != 0) {
        language_error_set(error, 0, 1, 1,
                           "unknown function '%s'", node->text);
        return false;
    }

    LiteralInfo literal = classify_literal(node->text);
    if (literal.kind == LITERAL_INVALID) {
        language_error_set(error, 0, 1, 1,
                           "unknown literal '%s'", node->text);
        return false;
    }

    if ((literal.kind == LITERAL_NUM_UP ||
         literal.kind == LITERAL_NUM_DOWN ||
         literal.kind == LITERAL_NUM_UP_STAR ||
         literal.kind == LITERAL_NUM_DOWN_STAR) && literal.number <= 0) {
        language_error_set(error, 0, 1, 1,
                           "arrow multiple must be positive");
        return false;
    }
    return true;
}

bool semantic_validate(const AstNode *root, LanguageError *error) {
    language_error_clear(error);
    return semantic_validate_node(root, error);
}

static Game *evaluate_node(const AstNode *node, LanguageError *error);

static Game *evaluate_literal(const char *text, LanguageError *error) {
    LiteralInfo info = classify_literal(text);
    switch (info.kind) {
        case LITERAL_INT:
            return make_int(info.number);
        case LITERAL_STAR:
            return game_star();
        case LITERAL_UP:
            return game_up();
        case LITERAL_DOWN:
            return game_down();
        case LITERAL_NUM_STAR:
            return game_add(make_int(info.number), game_star());
        case LITERAL_NIMBER:
            return make_nimber(info.number);
        case LITERAL_NUM_UP:
            return make_up_multiple(info.number, 0);
        case LITERAL_NUM_DOWN:
            return make_down_multiple(info.number, 0);
        case LITERAL_UP_STAR:
            return game_up_star();
        case LITERAL_DOWN_STAR:
            return game_down_star();
        case LITERAL_NUM_UP_STAR:
            return make_up_multiple(info.number, 1);
        case LITERAL_NUM_DOWN_STAR:
            return make_down_multiple(info.number, 1);
        case LITERAL_INVALID:
        default:
            language_error_set(error, 0, 1, 1,
                               "cannot evaluate literal '%s'", text);
            return NULL;
    }
}

static Game *subtract_games(Game *left, Game *right) {
    Game *negative = game_negate(right);
    return negative == NULL ? NULL : game_add(left, negative);
}

static Game *divide_games(Game *numerator,
                          Game *denominator,
                          LanguageError *error) {
    int result_numerator;
    int result_denominator;
    DyadicDivisionStatus status = dyadic_divide_games(
        numerator, denominator, &result_numerator, &result_denominator
    );

    switch (status) {
        case DYADIC_DIVISION_OK:
            return make_dyadic(result_numerator, result_denominator);
        case DYADIC_DIVISION_NOT_NUMBERS:
            language_error_set(error, 0, 1, 1,
                               "division is supported only for dyadic surreal numbers");
            break;
        case DYADIC_DIVISION_BY_ZERO:
            language_error_set(error, 0, 1, 1, "division by zero");
            break;
        case DYADIC_DIVISION_NON_DYADIC:
            language_error_set(error, 0, 1, 1,
                               "division result is not a short dyadic surreal number");
            break;
        case DYADIC_DIVISION_OVERFLOW:
            language_error_set(error, 0, 1, 1, "dyadic division overflow");
            break;
        case DYADIC_DIVISION_OUT_OF_RANGE:
            language_error_set(error, 0, 1, 1,
                               "division result is outside make_dyadic(int, int) range");
            break;
    }
    return NULL;
}

static Game *evaluate_game_constructor(const AstNode *node,
                                       LanguageError *error) {
    const AstNode *left_node = node->children[0];
    const AstNode *right_node = node->children[1];
    Game **left = NULL;
    Game **right = NULL;

    for (size_t i = 0; i < ast_node_child_count(left_node); i++) {
        Game *option = evaluate_node(left_node->children[i], error);
        if (option == NULL) goto fail;
        da_push(left, option);
    }
    for (size_t i = 0; i < ast_node_child_count(right_node); i++) {
        Game *option = evaluate_node(right_node->children[i], error);
        if (option == NULL) goto fail;
        da_push(right, option);
    }

    Game *result = game_canonicalize(game_from_games(left, right));
    da_free(left);
    da_free(right);
    return result;

fail:
    da_free(left);
    da_free(right);
    return NULL;
}

static Game *evaluate_function(const AstNode *node, LanguageError *error) {
    Game *argument = evaluate_node(node->children[0], error);
    if (argument == NULL) return NULL;

    if (strcmp(node->text, "canonical") == 0 ||
        strcmp(node->text, "canonicalize") == 0) {
        return game_canonicalize(argument);
    }
    if (strcmp(node->text, "cool") == 0) return cool_with_star(argument);
    if (strcmp(node->text, "projection") == 0 ||
        strcmp(node->text, "star_projection") == 0) {
        return star_projection(argument);
    }

    language_error_set(error, 0, 1, 1,
                       "unknown function '%s'", node->text);
    return NULL;
}

static Game *evaluate_node(const AstNode *node, LanguageError *error) {
    size_t count = ast_node_child_count(node);
    if (text_is(node, "{}")) return evaluate_game_constructor(node, error);

    if (text_is(node, "+")) {
        if (count == 1) return evaluate_node(node->children[0], error);
        Game *left = evaluate_node(node->children[0], error);
        Game *right = evaluate_node(node->children[1], error);
        return left == NULL || right == NULL ? NULL : game_add(left, right);
    }

    if (text_is(node, "-")) {
        if (count == 1) {
            Game *value = evaluate_node(node->children[0], error);
            return value == NULL ? NULL : game_negate(value);
        }
        Game *left = evaluate_node(node->children[0], error);
        Game *right = evaluate_node(node->children[1], error);
        return left == NULL || right == NULL
             ? NULL
             : subtract_games(left, right);
    }

    if (text_is(node, "*") && count == 2) {
        Game *left = evaluate_node(node->children[0], error);
        Game *right = evaluate_node(node->children[1], error);
        return left == NULL || right == NULL
             ? NULL
             : game_multiply(left, right);
    }

    if (text_is(node, "/")) {
        Game *left = evaluate_node(node->children[0], error);
        Game *right = evaluate_node(node->children[1], error);
        return left == NULL || right == NULL
             ? NULL
             : divide_games(left, right, error);
    }

    if (count == 0) return evaluate_literal(node->text, error);
    return evaluate_function(node, error);
}

Game *semantic_evaluate(const AstNode *root, LanguageError *error) {
    language_error_clear(error);
    if (!semantic_validate_node(root, error)) return NULL;

    Game *result = evaluate_node(root, error);
    if (result == NULL && error != NULL && error->message[0] == '\0') {
        language_error_set(error, 0, 1, 1, "evaluation failed");
    }
    return result;
}
