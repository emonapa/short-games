/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include "game_format.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../game_darray.h"
#include "../singletons.h"

#define INITIAL_BUFFER_CAPACITY 256u

static char *print_buffer;
static size_t print_length;
static size_t print_capacity;

static int buffer_reserve(size_t additional) {
    if (print_length == SIZE_MAX ||
        additional > SIZE_MAX - print_length - 1) {
        return 0;
    }

    size_t required = print_length + additional + 1;
    if (required <= print_capacity) return 1;

    size_t capacity = print_capacity != 0
                    ? print_capacity
                    : INITIAL_BUFFER_CAPACITY;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }

    char *resized = realloc(print_buffer, capacity);
    if (resized == NULL) return 0;

    print_buffer = resized;
    print_capacity = capacity;
    return 1;
}

static int buffer_reset(void) {
    print_length = 0;
    if (!buffer_reserve(0)) return 0;
    print_buffer[0] = '\0';
    return 1;
}

static int buffer_append(const char *text) {
    size_t length = strlen(text);
    if (!buffer_reserve(length)) return 0;

    memcpy(print_buffer + print_length, text, length + 1);
    print_length += length;
    return 1;
}

static int get_up_arrow_multiple(Game *game) {
    if (game == NULL) return 0;

    int count = 1;
    Game *current = game;
    while (current != NULL &&
           game_len(&current->left) == 1 &&
           game_len(&current->right) == 1 &&
           current->left[0] == game_zero()) {
        if (current->right[0] == game_up_star()) return count + 1;
        if (current->right[0] == game_star()) return count;
        current = current->right[0];
        count++;
    }
    return 0;
}

static int get_down_arrow_multiple(Game *game) {
    if (game == NULL) return 0;

    int count = 1;
    Game *current = game;
    while (current != NULL &&
           game_len(&current->left) == 1 &&
           game_len(&current->right) == 1 &&
           current->right[0] == game_zero()) {
        if (current->left[0] == game_down_star()) return count + 1;
        if (current->left[0] == game_star()) return count;
        current = current->left[0];
        count++;
    }
    return 0;
}

static int get_up_arrow_multiple_plus_star(Game *game) {
    if (game == game_up_star()) return 1;

    Game *current = game;
    while (current != NULL &&
           game_len(&current->left) == 1 &&
           game_len(&current->right) == 1 &&
           current->left[0] == game_zero()) {
        int inner = get_up_arrow_multiple(current->right[0]);
        if (inner > 0) return inner + 1;
        current = current->right[0];
    }
    return 0;
}

static int get_down_arrow_multiple_plus_star(Game *game) {
    if (game == game_down_star()) return 1;

    Game *current = game;
    while (current != NULL &&
           game_len(&current->left) == 1 &&
           game_len(&current->right) == 1 &&
           current->right[0] == game_zero()) {
        int inner = get_down_arrow_multiple(current->left[0]);
        if (inner > 0) return inner + 1;
        current = current->left[0];
    }
    return 0;
}

static int get_nimber_value(Game *game) {
    if (game == NULL) return -1;

    size_t left_count = game_len(&game->left);
    size_t right_count = game_len(&game->right);
    if (left_count == 0 && right_count == 0) return 0;
    if (left_count != right_count || left_count > (size_t)INT_MAX) return -1;

    int n = (int)left_count;
    unsigned char *found_left = calloc(left_count, sizeof(*found_left));
    unsigned char *found_right = calloc(right_count, sizeof(*found_right));
    if (found_left == NULL || found_right == NULL) {
        free(found_left);
        free(found_right);
        return -1;
    }

    int result = n;
    for (int i = 0; i < n; i++) {
        int left = get_nimber_value(game->left[i]);
        int right = get_nimber_value(game->right[i]);
        if (left < 0 || left >= n || right < 0 || right >= n) {
            result = -1;
            break;
        }
        found_left[left] = 1;
        found_right[right] = 1;
    }

    for (int i = 0; result >= 0 && i < n; i++) {
        if (!found_left[i] || !found_right[i]) result = -1;
    }

    free(found_left);
    free(found_right);
    return result;
}

static int is_base_plus_star(Game *game, Game *base) {
    return game != NULL && base != NULL &&
           game_len(&game->left) == 1 &&
           game_len(&game->right) == 1 &&
           game->left[0] == base && game->right[0] == base;
}

static int get_number_plus_down_arrows(Game *game, double *out_base) {
    if (game == NULL || game_len(&game->left) != 1 ||
        game_len(&game->right) != 1) {
        return 0;
    }

    Game *base = game->right[0];
    if (!is_number(base)) return 0;

    int count = 1;
    Game *current = game;
    while (current != NULL && game_len(&current->left) == 1 &&
           game_len(&current->right) == 1 && current->right[0] == base) {
        Game *next = current->left[0];
        if (next == NULL) return 0;

        if (game_len(&next->left) == 1 && next->left[0] == base) {
            if (game_len(&next->right) == 1 && next->right[0] == base) {
                return get_dyadic_value(base, out_base) ? count : 0;
            }

            if (game_len(&next->right) == 2) {
                int has_base = next->right[0] == base || next->right[1] == base;
                int has_star = is_base_plus_star(next->right[0], base) ||
                               is_base_plus_star(next->right[1], base);
                if (has_base && has_star) {
                    return get_dyadic_value(base, out_base) ? count + 1 : 0;
                }
            }
        }

        current = next;
        count++;
    }
    return 0;
}

static int get_number_plus_up_arrows(Game *game, double *out_base) {
    if (game == NULL || game_len(&game->left) != 1 ||
        game_len(&game->right) != 1) {
        return 0;
    }

    Game *base = game->left[0];
    if (!is_number(base)) return 0;

    int count = 1;
    Game *current = game;
    while (current != NULL && game_len(&current->left) == 1 &&
           game_len(&current->right) == 1 && current->left[0] == base) {
        Game *next = current->right[0];
        if (next == NULL) return 0;

        if (game_len(&next->right) == 1 && next->right[0] == base) {
            if (game_len(&next->left) == 1 && next->left[0] == base) {
                return get_dyadic_value(base, out_base) ? count : 0;
            }

            if (game_len(&next->left) == 2) {
                int has_base = next->left[0] == base || next->left[1] == base;
                int has_star = is_base_plus_star(next->left[0], base) ||
                               is_base_plus_star(next->left[1], base);
                if (has_base && has_star) {
                    return get_dyadic_value(base, out_base) ? count + 1 : 0;
                }
            }
        }

        current = next;
        count++;
    }
    return 0;
}

static int append_counted_symbol(int count,
                                 const char *one,
                                 const char *many_format) {
    char temporary[64];
    if (count == 1) return buffer_append(one);

    int length = snprintf(temporary, sizeof(temporary), many_format, count);
    return length >= 0 && (size_t)length < sizeof(temporary) &&
           buffer_append(temporary);
}

static int format_symbolic(Game *game, int *matched) {
    char temporary[64];
    double number;
    int count;

    *matched = 1;
    if (get_dyadic_value(game, &number)) {
        int length = snprintf(temporary, sizeof(temporary), "%g", number);
        return length >= 0 && (size_t)length < sizeof(temporary) &&
               buffer_append(temporary);
    }
    if (game == game_star()) return buffer_append("*");
    if (game == game_up_star()) return buffer_append("↑ + *");
    if (game == game_down_star()) return buffer_append("↓ + *");

    count = get_up_arrow_multiple_plus_star(game);
    if (count > 0) return append_counted_symbol(count, "↑ + *", "%d↑ + *");

    count = get_down_arrow_multiple_plus_star(game);
    if (count > 0) return append_counted_symbol(count, "↓ + *", "%d↓ + *");

    count = get_up_arrow_multiple(game);
    if (count > 0) return append_counted_symbol(count, "↑", "%d↑");

    count = get_down_arrow_multiple(game);
    if (count > 0) return append_counted_symbol(count, "↓", "%d↓");

    count = get_nimber_value(game);
    if (count > 0) return append_counted_symbol(count, "*", "*%d");

    count = get_number_plus_down_arrows(game, &number);
    if (count > 0) {
        int length = count == 1
                   ? snprintf(temporary, sizeof(temporary), "%g + ↓", number)
                   : snprintf(temporary, sizeof(temporary), "%g + %d↓", number, count);
        return length >= 0 && (size_t)length < sizeof(temporary) &&
               buffer_append(temporary);
    }

    count = get_number_plus_up_arrows(game, &number);
    if (count > 0) {
        int length = count == 1
                   ? snprintf(temporary, sizeof(temporary), "%g + ↑", number)
                   : snprintf(temporary, sizeof(temporary), "%g + %d↑", number, count);
        return length >= 0 && (size_t)length < sizeof(temporary) &&
               buffer_append(temporary);
    }

    if (is_dyadic_plus_star(game, &number)) {
        int length = snprintf(temporary, sizeof(temporary), "%g + *", number);
        return length >= 0 && (size_t)length < sizeof(temporary) &&
               buffer_append(temporary);
    }

    *matched = 0;
    return 1;
}

static int format_game(Game *game, enum output_format format) {
    if (game == NULL) return buffer_append("NULL");
    if (game == game_zero()) return buffer_append("0");

    if (format == FORMAT_FORMATED) {
        int matched;
        if (!format_symbolic(game, &matched)) return 0;
        if (matched) return 1;
    }

    if (!buffer_append("{")) return 0;
    for (size_t i = 0; i < game_len(&game->left); i++) {
        if (!format_game(game->left[i], format)) return 0;
        if (i + 1 < game_len(&game->left) && !buffer_append(", ")) return 0;
    }

    if (!buffer_append(" | ")) return 0;
    for (size_t i = 0; i < game_len(&game->right); i++) {
        if (!format_game(game->right[i], format)) return 0;
        if (i + 1 < game_len(&game->right) && !buffer_append(", ")) return 0;
    }
    return buffer_append("}");
}

const char *game_get_string(Game *game, enum output_format format) {
    if (game == NULL) return "NULL";
    if (!buffer_reset() || !format_game(game, format)) {
        return "<game formatting failed>";
    }
    return print_buffer;
}
