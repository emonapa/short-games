/*
 * Parser for textual short-game notation.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_string.h"
#include "singletons.h"
#include "game_darray.h"

#define GS_ERR_SIZE 256

static char gs_error[GS_ERR_SIZE];

static Game *parse_game(const char *begin, const char *end);

const char *game_string_last_error(void) {
    return gs_error;
}

static void set_error(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vsnprintf(gs_error, sizeof(gs_error), fmt, args);
    va_end(args);
}

static const char *skip_ws_left(const char *p, const char *end) {
    while (p < end && isspace((unsigned char)*p)) p++;
    return p;
}

static const char *skip_ws_right(const char *begin, const char *end) {
    while (end > begin && isspace((unsigned char)*(end - 1))) end--;
    return end;
}

static void trim_span(const char **begin, const char **end) {
    *begin = skip_ws_left(*begin, *end);
    *end = skip_ws_right(*begin, *end);
}

static int span_eq(const char *begin, const char *end, const char *lit) {
    size_t len = (size_t)(end - begin);
    return strlen(lit) == len && memcmp(begin, lit, len) == 0;
}

static char *span_to_cstr(const char *begin, const char *end) {
    size_t len = (size_t)(end - begin);
    char *s = (char *)malloc(len + 1);
    if (!s) {
        set_error("Out of memory while parsing");
        return NULL;
    }

    memcpy(s, begin, len);
    s[len] = '\0';
    return s;
}

static int parse_int_span(const char *begin, const char *end, int *out) {
    char *s = NULL;
    char *tail = NULL;
    long value;

    trim_span(&begin, &end);
    if (begin == end) return 0;

    s = span_to_cstr(begin, end);
    if (!s) return 0;

    errno = 0;
    value = strtol(s, &tail, 10);

    if (errno != 0 || tail == s || *tail != '\0' || value < INT_MIN || value > INT_MAX) {
        free(s);
        return 0;
    }

    free(s);
    *out = (int)value;
    return 1;
}

static const char *find_top_char(const char *begin, const char *end, char needle) {
    int depth = 0;

    for (const char *p = begin; p < end; p++) {
        if (*p == '{') {
            depth++;
        } else if (*p == '}') {
            depth--;
            if (depth < 0) return NULL;
        } else if (*p == needle && depth == 0) {
            return p;
        }
    }

    return NULL;
}

static const char *find_arrow(const char *begin, const char *end, size_t *arrow_len) {
    for (const char *p = begin; p < end; p++) {
        if (*p == '^' || *p == 'v') {
            *arrow_len = 1;
            return p;
        }

        if ((unsigned char)*p == 0xE2 && end - p >= 3) {
            if ((unsigned char)p[1] == 0x86 &&
                ((unsigned char)p[2] == 0x91 || (unsigned char)p[2] == 0x93)) {
                *arrow_len = 3;
                return p;
            }
        }
    }

    return NULL;
}

static int arrow_is_up(const char *arrow) {
    if (*arrow == '^') return 1;
    if (*arrow == 'v') return 0;
    return (unsigned char)arrow[2] == 0x91;
}

static int suffix_is_star(const char *begin, const char *end) {
    trim_span(&begin, &end);

    if (span_eq(begin, end, "*")) return 1;

    if (end - begin >= 3 && *begin == '+') {
        begin++;
        trim_span(&begin, &end);
        return span_eq(begin, end, "*");
    }

    return 0;
}

static Game **parse_game_list(const char *begin, const char *end) {
    Game **items = NULL;
    const char *part = begin;
    int depth = 0;

    trim_span(&begin, &end);
    if (begin == end) return NULL;

    part = begin;
    for (const char *p = begin; p < end; p++) {
        if (*p == '{') {
            depth++;
        } else if (*p == '}') {
            depth--;
            if (depth < 0) {
                set_error("Unbalanced braces in list");
                game_free(&items);
                return NULL;
            }
        } else if (*p == ',' && depth == 0) {
            Game *g = parse_game(part, p);
            if (!g) {
                game_free(&items);
                return NULL;
            }
            game_push(&items, g);
            part = p + 1;
        }
    }

    if (depth != 0) {
        set_error("Unbalanced braces in list");
        game_free(&items);
        return NULL;
    }

    Game *g = parse_game(part, end);
    if (!g) {
        game_free(&items);
        return NULL;
    }
    game_push(&items, g);

    return items;
}

static Game *parse_braced_game(const char *begin, const char *end) {
    const char *inner_begin = begin + 1;
    const char *inner_end = end - 1;
    const char *pipe = find_top_char(inner_begin, inner_end, '|');
    Game **left = NULL;
    Game **right = NULL;
    Game *result = NULL;

    if (!pipe) {
        set_error("Missing top-level '|' in game expression");
        return NULL;
    }

    left = parse_game_list(inner_begin, pipe);
    if (gs_error[0] != '\0') return NULL;

    right = parse_game_list(pipe + 1, inner_end);
    if (gs_error[0] != '\0') {
        game_free(&left);
        return NULL;
    }

    result = game_canonicalize(game_from_games(left, right));

    game_free(&left);
    game_free(&right);

    return result;
}

static Game *parse_fraction(const char *begin, const char *end, const char *slash) {
    int p;
    int q;

    if (!parse_int_span(begin, slash, &p) || !parse_int_span(slash + 1, end, &q)) {
        set_error("Can't parse a fraction");
        return NULL;
    }

    Game *g = make_dyadic(p, q);
    if (!g) set_error("Invalid dyadic fraction: denominator must be a positive power of 2");
    return g;
}

static Game *parse_arrow_game(const char *begin, const char *end, const char *arrow, size_t arrow_len) {
    const char *pre_begin = begin;
    const char *pre_end = arrow;
    const char *suffix_begin = arrow + arrow_len;
    const char *suffix_end = end;
    int mult = 1;
    int with_star = 0;

    trim_span(&pre_begin, &pre_end);
    trim_span(&suffix_begin, &suffix_end);

    if (pre_begin != pre_end && !parse_int_span(pre_begin, pre_end, &mult)) {
        set_error("Invalid arrow multiplier");
        return NULL;
    }

    if (mult <= 0) {
        set_error("Arrow multiplier must be positive");
        return NULL;
    }

    if (suffix_begin != suffix_end) {
        if (!suffix_is_star(suffix_begin, suffix_end)) {
            set_error("Unexpected suffix after arrow");
            return NULL;
        }
        with_star = 1;
    }

    return arrow_is_up(arrow) ? make_up_multiple(mult, with_star)
                             : make_down_multiple(mult, with_star);
}

static Game *parse_game(const char *begin, const char *end) {
    const char *slash;
    const char *arrow;
    size_t arrow_len = 0;
    int n;

    trim_span(&begin, &end);

    if (begin == end) {
        set_error("Empty input");
        return NULL;
    }

    if (*begin == '{' && *(end - 1) == '}') {
        return parse_braced_game(begin, end);
    }

    if (span_eq(begin, end, "*")) return game_star();

    if (*begin == '*' && end - begin > 1) {
        if (!parse_int_span(begin + 1, end, &n) || n < 0) {
            set_error("Invalid nimber");
            return NULL;
        }
        return make_nimber(n);
    }

    arrow = find_arrow(begin, end, &arrow_len);
    if (arrow) return parse_arrow_game(begin, end, arrow, arrow_len);

    slash = find_top_char(begin, end, '/');
    if (slash) return parse_fraction(begin, end, slash);

    if (parse_int_span(begin, end, &n)) return make_int(n);

    set_error("Unable to parse game expression");
    return NULL;
}

Game *game_from_string(const char *text) {
    const char *begin = text;
    const char *end;

    gs_error[0] = '\0';

    if (!text) {
        set_error("Input string is NULL");
        return NULL;
    }

    end = text + strlen(text);
    return parse_game(begin, end);
}
