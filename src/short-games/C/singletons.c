/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hackenbushe s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hackenbush Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#include "singletons.h"
#include "short_game.h"

static Game *val_zero = NULL;
static Game *val_star = NULL;
static Game *val_one  = NULL;
static Game *val_up   = NULL;
static Game *val_down = NULL;
static Game *val_up_star   = NULL;
static Game *val_down_star = NULL;

static char print_buffer[65536];


void singletons_init(void) {
    // 0 = { | }
    val_zero = game_canonicalize(game_make(NULL, 0, NULL, 0));

    // * = { 0 | 0 }
    Game *arr_zero[] = { val_zero };
    val_star = game_canonicalize(game_make(arr_zero, 1, arr_zero, 1));

    // 1 = { 0 | }
    val_one = game_canonicalize(game_make(arr_zero, 1, NULL, 0));

    // ↑ = { 0 | * }
    Game *arr_star[] = { val_star };
    val_up = game_canonicalize(game_make(arr_zero, 1, arr_star, 1));

    // ↓ = { * | 0 }
    val_down = game_canonicalize(game_make(arr_star, 1, arr_zero, 1));

    // ↑* = ↑ + * = { 0, * | 0 }
    Game *arr_zero_star[] = { val_zero, val_star };
    val_up_star = game_canonicalize(game_make(arr_zero_star, 2, arr_zero, 1));

    // ↓* = ↓ + * = { 0 | 0, * }
    val_down_star = game_canonicalize(game_make(arr_zero, 1, arr_zero_star, 2));
}

// -----------------------------------------------------------------
// Getters
// -----------------------------------------------------------------
Game* game_zero(void) { return val_zero; }
Game* game_star(void) { return val_star; }
Game* game_one(void)  { return val_one; }
Game* game_up(void)   { return val_up; }
Game* game_down(void) { return val_down; }


int is_zero(Game *G) { return G == val_zero; }
int is_star(Game *G) { return G == val_star; }
int is_one(Game *G)  { return G == val_one; }

// -----------------------------------------------------------------
// Arrow multiples
// -----------------------------------------------------------------
static int get_up_arrow_multiple(Game *G) {
    if (!G) return 0;
    int count = 1;
    Game *curr = G;

    while (1) {
        if (curr->L_count != 1 || curr->R_count != 1) return 0;

        if (curr->left[0] != val_zero) return 0;
        if (curr->right[0] == val_up_star) return count + 1;
        if (curr->right[0] == val_star) return count;

        curr = curr->right[0];
        count++;
    }
    return 0;
}

static int get_down_arrow_multiple(Game *G) {
    if (!G) return 0;
    int count = 1;
    Game *curr = G;

    while (1) {
        if (curr->L_count != 1 || curr->R_count != 1) return 0;

        if (curr->right[0] != val_zero) return 0;
        if (curr->left[0] == val_down_star) return count + 1;
        if (curr->left[0] == val_star) return count;

        curr = curr->left[0];
        count++;
    }
    return 0;
}


// -----------------------------------------------------------------
// Nimber functions
// -----------------------------------------------------------------
// Returns 'n' for the value *n, where *0 = 0, or -1 if this is not a nimber.
static int get_nimber_value(Game *G) {
    if (!G) return -1;
    if (G->L_count == 0 && G->R_count == 0) return 0; // Zero is *0.

    // A nimber must have the same number of Left and Right options.
    if (G->L_count != G->R_count) return -1;

    int n = G->L_count;

    // Track whether all values from 0 to n - 1 were found.
    int *found_L = (int*)calloc(n, sizeof(int));
    int *found_R = (int*)calloc(n, sizeof(int));
    int is_nimber = 1;

    for (int i = 0; i < n; i++) {
        int vL = get_nimber_value(G->left[i]);
        int vR = get_nimber_value(G->right[i]);

        // If a child is not a nimber, or its value is too large, this cannot be *n.
        if (vL < 0 || vL >= n) { is_nimber = 0; break; }
        if (vR < 0 || vR >= n) { is_nimber = 0; break; }

        found_L[vL] = 1;
        found_R[vR] = 1;
    }

    // Check that there are no gaps, for example 0 and *2 without *1.
    if (is_nimber) {
        for (int i = 0; i < n; i++) {
            if (!found_L[i] || !found_R[i]) {
                is_nimber = 0;
                break;
            }
        }
    }

    free(found_L);
    free(found_R);

    return is_nimber ? n : -1;
}

// These surreal numbers are not expected to be deeply nested
// relative to the solver limits, so recursion is acceptable here.
// -----------------------------------------------------------------
// Helper functions
// -----------------------------------------------------------------
// Returns 1 if the canonical game is a dyadic or integer number.
int is_number(Game *G) {
    if (!G) return 0;
    if (G->L_count == 0 && G->R_count == 0) return 1;
    if (G->L_count > 1 || G->R_count > 1) return 0;

    if (G->L_count == 1 && !is_number(G->left[0])) return 0;
    if (G->R_count == 1 && !is_number(G->right[0])) return 0;

    // Number condition: no GL >= GR may hold.
    // Without this check, for example * = {0|0} would be treated as a number.
    if (G->L_count == 1 && G->R_count == 1)
        if (game_geq(G->left[0], G->right[0])) return 0;

    return 1;
}

// Returns 1 if the game is a number and stores its value in *out_val.
int get_dyadic_value(Game *G, double *out_val) {
    if (!G) return 0;

    // Base case.
    if (G == game_zero()) {
        *out_val = 0.0;
        return 1;
    }

    // A canonical number must have at most one option on each side.
    // This only works when the number is in canonical form, meaning
    // there are no dominated options in this case.
    if (G->L_count > 1 || G->R_count > 1) return 0;

    double l_val = 0.0, r_val = 0.0;
    int has_L = 0, has_R = 0;

    // A Left option, if present, must also be a number.
    if (G->L_count == 1) {
        if (!get_dyadic_value(G->left[0], &l_val)) return 0;
        has_L = 1;
    }
    // A Right option, if present, must also be a number.
    if (G->R_count == 1) {
        if (!get_dyadic_value(G->right[0], &r_val)) return 0;
        has_R = 1;
    }

    // Compute the value using Conway's rules for numbers.
    if (has_L && !has_R) {
        *out_val = l_val + 1.0;  // Addition, for example {0 | } = 1.
        return 1;
    } else if (!has_L && has_R) {
        *out_val = r_val - 1.0;  // Subtraction, for example { | 0} = -1.
        return 1;
    } else if (has_L && has_R) {
        // For numbers, the left option must be smaller than the right option.
        if (l_val >= r_val) return 0;

        // This only works when the number is in canonical form,
        // meaning there are no reversible options in this case.
        *out_val = (l_val + r_val) / 2.0; // Fraction, for example {0 | 1} = 0.5.
        return 1;
    }

    return 0;
}


// Is this game exactly base + *, i.e. {base | base}?
int is_base_plus_star(Game *G, Game *base) {
    if (!G || !base) return 0;
    return (G->L_count == 1 && G->R_count == 1 &&
            G->left[0] == base && G->right[0] == base);
}

// The pattern-based algorithms below can probably be proven formally,
// but they were derived by listing the first few values and identifying patterns.
// -----------------------------------------------------------------
// 1. Dyadic number + multiples of down arrow (X + n↓)
// -----------------------------------------------------------------
static int get_number_plus_down_arrows(Game *G, double *out_base_val) {
    // Must have exactly one option on each side.
    if (!G || G->L_count != 1 || G->R_count != 1) return 0;

    Game *base = G->right[0];
    if (!is_number(base)) return 0; // The base must be a number.

    int count = 1;
    Game *curr = G;

    while (1) {
        if (curr->L_count != 1 || curr->R_count != 1) return 0;
        if (curr->right[0] != base) return 0; // The base must remain the same.

        Game *next = curr->left[0]; // Descend to the left.

        if (next->L_count == 1 && next->left[0] == base) {

            // a) next->right == base, which is {base | base}, exactly base + *.
            if (next->R_count == 1 && next->right[0] == base) {
                get_dyadic_value(base, out_base_val);
                return count;
            }

            // b) next->right == {base, {base|base}}.
            if (next->R_count == 2) {
                int has_base = (next->right[0] == base || next->right[1] == base);
                int has_star = (is_base_plus_star(next->right[0], base) || is_base_plus_star(next->right[1], base));
                if (has_base && has_star) {
                    get_dyadic_value(base, out_base_val);
                    return count + 1;
                }
            }
        }

        curr = next;
        count++;
    }
    return 0;
}



static int get_number_plus_up_arrows(Game *G, double *out_base_val) {
    // Must have exactly one option on each side.
    if (!G || G->L_count != 1 || G->R_count != 1) return 0;

    // For the up arrow (↑ = {0 | *}), the base number is always on the left.
    Game *base = G->left[0];
    if (!is_number(base)) return 0;

    int count = 1;
    Game *curr = G;

    while (1) {
        if (curr->L_count != 1 || curr->R_count != 1) return 0;
        if (curr->left[0] != base) return 0; // The base is now on the left.

        Game *next = curr->right[0]; // Descend to the right.

        if (next->R_count == 1 && next->right[0] == base) {

            // next->left == base, which is {base | base}.
            if (next->L_count == 1 && next->left[0] == base) {
                get_dyadic_value(base, out_base_val);
                return count;
            }

            // next->left == {base, {base|base}}.
            if (next->L_count == 2) {
                int has_base = (next->left[0] == base || next->left[1] == base);
                int has_star = (is_base_plus_star(next->left[0], base) || is_base_plus_star(next->left[1], base));
                if (has_base && has_star) {
                    get_dyadic_value(base, out_base_val);
                    return count + 1;
                }
            }
        }

        curr = next;
        count++;
    }
    return 0;
}


int is_dyadic_plus_star(Game *G, double *out_dyadic_val) {
    if (G->L_count == 1 && G->R_count == 1 && G->left[0] == G->right[0]) {
        if(get_dyadic_value(G->left[0], out_dyadic_val)) return 1;
    }
    return 0;
}

static void buffer_append(const char *text) {
    size_t current_len = strlen(print_buffer);
    size_t max_len = sizeof(print_buffer) - 1;
    if (current_len < max_len) {
        strncat(print_buffer, text, max_len - current_len);
    }
}

// Writes the value of a surreal number to the buffer.
static void game_get_string_recursive(Game *G, enum output_format format) {
    if (!G) return;

    if (G == val_zero) { buffer_append("0"); return; }
    if (format == FORMAT_FORMATED) {
        char temp[64];

        double num_val;
        if (get_dyadic_value(G, &num_val)) {
            snprintf(temp, sizeof(temp), "%g", num_val);
            buffer_append(temp);
            return;
        }

        if      (G == val_star) { buffer_append("*"); return; }
        else if (G == val_one)  { buffer_append("1"); return; }
        else if (G == val_up_star)   { buffer_append("↑ + *"); return; }
        else if (G == val_down_star) { buffer_append("↓ + *"); return; }

        // Arrow multiples.
        int up_arrows = get_up_arrow_multiple(G);
        if (up_arrows > 0) {
            if (up_arrows == 1) snprintf(temp, sizeof(temp), "↑");
            else snprintf(temp, sizeof(temp), "%d↑", up_arrows);
            buffer_append(temp);
            return;
        }

        int down_arrows = get_down_arrow_multiple(G);
        if (down_arrows > 0) {
            if (down_arrows == 1) snprintf(temp, sizeof(temp), "↓");
            else snprintf(temp, sizeof(temp), "%d↓", down_arrows);
            buffer_append(temp);
            return;
        }

        // Nimber multiples (*).
        int nimbers = get_nimber_value(G);
        if (nimbers > 0) {
            if (nimbers == 1) snprintf(temp, sizeof(temp), "*");
            else snprintf(temp, sizeof(temp), "*%d", nimbers);
            buffer_append(temp);
            return;
        }

        // Number + X arrows.
        double base_val;

        int n_down = get_number_plus_down_arrows(G, &base_val);
        if (n_down > 0) {
            if (n_down == 1) snprintf(temp, sizeof(temp), "%g + ↓", base_val);
            else snprintf(temp, sizeof(temp), "%g + %d↓", base_val, n_down);
            buffer_append(temp);
            return;
        }

        int n_up = get_number_plus_up_arrows(G, &base_val);
        if (n_up > 0) {
            if (n_up == 1) snprintf(temp, sizeof(temp), "%g + ↑", base_val);
            else snprintf(temp, sizeof(temp), "%g + %d↑", base_val, n_up);
            buffer_append(temp);
            return;
        }

        // Dyadic number + *.
        if (G->L_count == 1 && G->R_count == 1 && G->left[0] == G->right[0]) {
            if(get_dyadic_value(G->left[0], &num_val)) {
                snprintf(temp, sizeof(temp), "%g + *", num_val);
                buffer_append(temp);
                return;
            }
        }
    }

    // Fallback.
    buffer_append("{");
    for (int i = 0; i < G->L_count; i++) {
        game_get_string_recursive(G->left[i], format);
        if (i < G->L_count - 1) buffer_append(", ");
    }
    buffer_append(" | ");
    for (int i = 0; i < G->R_count; i++) {
        game_get_string_recursive(G->right[i], format);
        if (i < G->R_count - 1) buffer_append(", ");
    }
    buffer_append("}");
}

// Wrapper for Python bindings.
const char* game_get_string(Game *G, enum output_format format) {
    print_buffer[0] = '\0';
    if (!G) return "NULL";
    game_get_string_recursive(G, format);
    return print_buffer;
}

/*
    =======================================================================
                             Calculator functions
    =======================================================================
*/
// -- make_int --------------------------------------------------------------
// Build integer n as a canonical Game*.
Game* make_int(int n) {
    if (n == 0) return game_zero();

    if (n > 0) {
        Game *prev  = make_int(n - 1);
        Game *arr[] = { prev };
        return game_canonicalize(game_make(arr, 1, NULL, 0));
    } else {
        Game *prev  = make_int(n + 1);
        Game *arr[] = { prev };
        return game_canonicalize(game_make(NULL, 0, arr, 1));
    }
}


// -- make_dyadic -----------------------------------------------------------
// Build dyadic rational p/q.  q must be a positive power of 2.
// Returns NULL on bad input.
Game* make_dyadic(int p, int q) {
    if (q <= 0 || (q & (q - 1)) != 0) return NULL;
    if (q == 1) return make_int(p);

    // Reduce the fraction.
    if ((p & 1) == 0) return make_dyadic(p / 2, q / 2);

    // Conway form.
    int half_q = q / 2;
    int k = (p - 1) / 2;

    Game *left  = make_dyadic(k,     half_q);
    Game *right = make_dyadic(k + 1, half_q);
    if (!left || !right) return NULL;

    Game *l_arr[] = { left };
    Game *r_arr[] = { right };

    return game_canonicalize(game_make(l_arr, 1, r_arr, 1));
}


// -- make_nimber -----------------------------------------------------------
// Build nimber *n.  *0 = 0, *1 = *, *2 = {0,* | 0,*}, etc.
Game* make_nimber(int n) {
    if (n < 0) return NULL;
    if (n == 0) return game_zero();
    if (n == 1) return game_star();

    Game **opts = (Game **)malloc((size_t)n * sizeof(Game *));
    if (!opts) return NULL;

    for (int i = 0; i < n; i++) opts[i] = make_nimber(i);

    Game *result = game_canonicalize(game_make(opts, n, opts, n));
    free(opts);
    return result;
}


// -- make_up_multiple ------------------------------------------------------
// Build n*↑, optionally adding * at the end.
// n must be >= 1.  with_star: 0 or 1.
Game* make_up_multiple(int n, int with_star) {
    if (n <= 0) return NULL;

    Game *base   = game_up();
    Game *result = base;

    for (int i = 1; i < n; i++) result = game_add(result, base);
    if (with_star)              result = game_add(result, game_star());

    return result;
}


// -- make_down_multiple ----------------------------------------------------
// Build n*↓, optionally adding * at the end.
// n must be >= 1.  with_star: 0 or 1.
Game* make_down_multiple(int n, int with_star) {
    if (n <= 0) return NULL;

    Game *base   = game_down();
    Game *result = base;

    for (int i = 1; i < n; i++) result = game_add(result, base);
    if (with_star)              result = game_add(result, game_star());

    return result;
}


Game* game_negate(Game *G) {
    if (G == NULL) return NULL;

    Game **new_left  = NULL;
    Game **new_right = NULL;

    if (G->R_count > 0) {
        new_left = malloc(sizeof(Game*) * G->R_count);
        if (!new_left) {
            warning("Malloc failed in game_negate.\n");
            return NULL;
        }
    }
    if (G->L_count > 0) {
        new_right = malloc(sizeof(Game*) * G->L_count);
        if (!new_right) {
            free(new_left);
            warning("Malloc failed in game_negate.\n");
            return NULL; }
    }

    for (int i = 0; i < G->R_count; i++) {
        new_left[i] = game_negate(G->right[i]);
        if (!new_left[i]) { free(new_left); free(new_right); return NULL; }
    }
    for (int i = 0; i < G->L_count; i++) {
        new_right[i] = game_negate(G->left[i]);
        if (!new_right[i]) { free(new_left); free(new_right); return NULL; }
    }

    Game *res = game_canonicalize(
        game_make(new_left, G->R_count, new_right, G->L_count)
    );
    free(new_left);
    free(new_right);
    return res;
}

/* ------------------------------------------------------------
   Cooling a game with star: G_*
   Definition:
     G_* = G                         if G is a number
     G_* = { G*_L + * | G*_R + * }  otherwise
   ------------------------------------------------------------ */
Game* cool_with_star(Game *G) {
    if (G == NULL) error_exit(ERR_NULL_POINTER, "");

    if (is_number(G)) return G;

    Game **new_left  = NULL;
    Game **new_right = NULL;
    Game *star = game_star();

    if (G->L_count > 0) {
        new_left = (Game**)malloc((size_t)G->L_count * sizeof(Game*));
        if (new_left == NULL) error_exit(ERR_MALLOC, "");
        for (int i = 0; i < G->L_count; i++)
            new_left[i] = game_add(cool_with_star(G->left[i]), star);
    }

    if (G->R_count > 0) {
        new_right = (Game**)malloc((size_t)G->R_count * sizeof(Game*));
        if (new_right == NULL) error_exit(ERR_MALLOC, "");
        for (int i = 0; i < G->R_count; i++)
            new_right[i] = game_add(cool_with_star(G->right[i]), star);
    }

    Game *result = game_canonicalize(
        game_make(new_left, G->L_count, new_right, G->R_count)
    );

    if (new_left)  free(new_left);
    if (new_right) free(new_right);

    return result;
}


/* ------------------------------------------------------------
   Star projection of a game H: p(H)
   Definition:
     p(H) = x                       if H = x or H = x + *, where x is a number
     p(H) = { p(H0^L) | p(H0^R) }  otherwise  (H0 is the canonical form of H)
   ------------------------------------------------------------ */
Game* star_projection(Game *H) {
    if (H == NULL) error_exit(ERR_NULL_POINTER, "");

    double val;

    // H = x (dyadic number).
    if (get_dyadic_value(H, &val))
        return H;

    // H = x + *.
    if (is_dyadic_plus_star(H, &val))
        return game_add(H, game_star()); // x + * + * = x.

    // General case: { p(H^L) | p(H^R) } over the canonical form.
    Game *H0 = game_canonicalize(H);

    Game **new_left  = NULL;
    Game **new_right = NULL;

    if (H0->L_count > 0) {
        new_left = (Game**)malloc((size_t)H0->L_count * sizeof(Game*));
        if (new_left == NULL) error_exit(ERR_MALLOC, "");
        for (int i = 0; i < H0->L_count; i++)
            new_left[i] = star_projection(H0->left[i]);
    }

    if (H0->R_count > 0) {
        new_right = (Game**)malloc((size_t)H0->R_count * sizeof(Game*));
        if (new_right == NULL) error_exit(ERR_MALLOC, "");
        for (int i = 0; i < H0->R_count; i++)
            new_right[i] = star_projection(H0->right[i]);
    }

    Game *result = game_canonicalize(
        game_make(new_left, H0->L_count, new_right, H0->R_count)
    );

    if (new_left)  free(new_left);
    if (new_right) free(new_right);

    return result;
}
