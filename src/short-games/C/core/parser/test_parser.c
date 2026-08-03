/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "game_format.h"
#include "game_string.h"
#include "language_error.h"
#include "parser.h"
#include "../game_darray.h"
#include "../singletons.h"

static int expect_valid(const char *source) {
    LanguageError error;
    AstNode *root = parser_parse_text(source, &error);
    if (root == NULL) {
        fprintf(stderr, "expected valid: %s\n", source);
        fprintf(stderr, "  line %zu, column %zu: %s\n",
                error.line, error.column, error.message);
        return 1;
    }

    ast_node_free(root);
    return 0;
}

static int expect_invalid(const char *source) {
    LanguageError error;
    AstNode *root = parser_parse_text(source, &error);
    if (root != NULL) {
        fprintf(stderr, "expected invalid: %s\n", source);
        ast_node_free(root);
        return 1;
    }
    return 0;
}

static int expect_error_location(const char *source,
                                 size_t expected_line,
                                 size_t expected_column) {
    LanguageError error;
    AstNode *root = parser_parse_text(source, &error);
    if (root != NULL || error.line != expected_line ||
        error.column != expected_column) {
        fprintf(stderr, "unexpected error location: %s\n", source);
        fprintf(stderr, "  expected: %zu:%zu\n",
                expected_line, expected_column);
        fprintf(stderr, "  actual:   %zu:%zu\n", error.line, error.column);
        ast_node_free(root);
        return 1;
    }
    return 0;
}

static int expect_value(const char *source, const char *expected) {
    Game *game = game_from_string(source);
    if (game == NULL) {
        fprintf(stderr, "expected evaluation to succeed: %s\n", source);
        fprintf(stderr, "  %s\n", game_string_last_error());
        return 1;
    }

    const char *actual = game_get_string(game, FORMAT_FORMATED);
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "unexpected value: %s\n", source);
        fprintf(stderr, "  expected: %s\n", expected);
        fprintf(stderr, "  actual:   %s\n", actual);
        return 1;
    }
    return 0;
}

static int expect_evaluation_error(const char *source,
                                   const char *message_fragment) {
    Game *game = game_from_string(source);
    if (game != NULL) {
        fprintf(stderr, "expected evaluation to fail: %s\n", source);
        fprintf(stderr, "  actual value: %s\n",
                game_get_string(game, FORMAT_FORMATED));
        return 1;
    }

    const char *message = game_string_last_error();
    if (strstr(message, message_fragment) == NULL) {
        fprintf(stderr, "unexpected error for: %s\n", source);
        fprintf(stderr, "  expected fragment: %s\n", message_fragment);
        fprintf(stderr, "  actual:            %s\n", message);
        return 1;
    }
    return 0;
}

static int expect_large_format(void) {
    const size_t option_count = 22000;
    Game **options = NULL;
    for (size_t i = 0; i < option_count; i++) {
        game_push(&options, game_zero());
    }

    Game *wide_game = game_from_games(options, NULL);
    game_free(&options);

    const char *formatted = game_get_string(wide_game, FORMAT_RAW);
    size_t expected_length = 3 * option_count + 3;
    size_t actual_length = strlen(formatted);
    int failed = actual_length != expected_length ||
                 formatted[actual_length - 1] != '}';
    if (failed) {
        fprintf(stderr, "large game formatting was truncated\n");
        fprintf(stderr, "  expected length: %zu\n", expected_length);
        fprintf(stderr, "  actual length:   %zu\n", actual_length);
    }

    game_free(&wide_game->left);
    game_free(&wide_game->right);
    free(wide_game);
    return failed;
}

static int gcd_positive(int left, int right) {
    while (right != 0) {
        int remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

static int test_small_integer_arithmetic(void) {
    int failures = 0;
    char source[64];

    for (int left = -4; left <= 4; left++) {
        for (int right = -4; right <= 4; right++) {
            snprintf(source, sizeof(source), "(%d) * (%d)", left, right);
            Game *actual = game_from_string(source);
            Game *expected = make_int(left * right);
            if (actual == NULL || !game_eq(actual, expected)) {
                fprintf(stderr, "integer product failed: %s\n", source);
                failures++;
            }
        }
    }

    for (int numerator = -8; numerator <= 8; numerator++) {
        for (int denominator = -8; denominator <= 8; denominator++) {
            if (denominator == 0) continue;

            snprintf(source, sizeof(source), "(%d) / (%d)",
                     numerator, denominator);
            Game *actual = game_from_string(source);

            int divisor = gcd_positive(abs(numerator), abs(denominator));
            int reduced_numerator = numerator / divisor;
            int reduced_denominator = denominator / divisor;
            if (reduced_denominator < 0) {
                reduced_numerator = -reduced_numerator;
                reduced_denominator = -reduced_denominator;
            }

            int is_dyadic = (reduced_denominator &
                              (reduced_denominator - 1)) == 0;
            if (is_dyadic) {
                Game *expected = make_dyadic(reduced_numerator,
                                             reduced_denominator);
                if (actual == NULL || !game_eq(actual, expected)) {
                    fprintf(stderr, "integer division failed: %s\n", source);
                    failures++;
                }
            } else if (actual != NULL) {
                fprintf(stderr, "non-dyadic division was accepted: %s\n", source);
                failures++;
            }
        }
    }
    return failures;
}

int main(void) {
    const char *valid[] = {
        "{ | }",
        "{0, *2 | ↑*, 2↓}",
        "-1 + 2 * 3",
        "* * *",
        "2*3",
        "2*",
        "2↑*",
        "canonical({0 | 1})",
        "f()",
        "f(1, 2)",
        "{cool(↑) | projection(↓)}",
        "+-+-1",
        "(1 + 2) / 3",
        "value(1)"
    };

    const char *invalid[] = {
        "",
        "{}",
        "{|",
        "{,1 | }",
        "{1, | }",
        "1 +",
        "(1 + 2",
        "1 2",
        "foo",
        "@",
        "f(,1)",
        "f(1,)",
        "2 / / 3"
    };

    int failures = 0;
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        failures += expect_valid(valid[i]);
    }
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        failures += expect_invalid(invalid[i]);
    }
    failures += expect_error_location("↑ @", 1, 3);

    short_game_init(0.01f);

    const struct {
        const char *source;
        const char *expected;
    } evaluations[] = {
        { "{ | }", "0" },
        { "1 + 1", "2" },
        { "{0 | 1}", "0.5" },
        { "2 * {0 | 1}", "1" },
        { "{0 | 1} / 2", "0.25" },
        { "3 / 6", "0.5" },
        { "3 / 12", "0.25" },
        { "(3 / 2) / 3", "0.5" },
        { "1 / {0 | 1}", "2" },
        { "{0 | 1} / {0 | 1}", "1" },
        { "-1 / 2", "-0.5" },
        { "-1 / -2", "0.5" },
        { "*2 + *2", "0" },
        { "* * *", "*" },
        { "-2 * 3", "-6" },
        { "{0 | 1} * {0 | 1}", "0.25" },
        { "-1 * ↑", "↓" },
        { "2↑ + ↓", "↑" },
        { "2↑*", "2↑ + *" },
        { "2↓*", "2↓ + *" }
    };

    for (size_t i = 0; i < sizeof(evaluations) / sizeof(evaluations[0]); i++) {
        failures += expect_value(evaluations[i].source, evaluations[i].expected);
    }

    failures += expect_evaluation_error("1 / 0", "division by zero");
    failures += expect_evaluation_error("1 / 3", "not a short dyadic");
    failures += expect_evaluation_error("2 / 6", "not a short dyadic");
    failures += expect_evaluation_error("* / 1", "only for dyadic surreal");
    failures += expect_evaluation_error("canonical(1, 2)",
                                        "expects exactly one argument");
    failures += expect_evaluation_error("unknown(1)", "unknown function");
    failures += expect_evaluation_error("0↑", "arrow multiple must be positive");
    failures += expect_large_format();
    failures += test_small_integer_arithmetic();

    short_game_free();

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
