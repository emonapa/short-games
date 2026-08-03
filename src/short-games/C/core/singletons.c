/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include <stdlib.h>

#include "../shared/error.h"

#include "singletons.h"
#include "short_game.h"
#include "game_intern_cache.h"
#include "game_darray.h"

static Game *val_zero = NULL;
static Game *val_star = NULL;
static Game *val_one  = NULL;
static Game *val_up   = NULL;
static Game *val_down = NULL;
static Game *val_up_star   = NULL;
static Game *val_down_star = NULL;

void singletons_init(void) {
    // 0 = { | }
    val_zero = game_intern_cache_prep_and_get(game_new());

    // * = { 0 | 0 }
    val_star = game_intern_cache_prep_and_get(game_from_game(val_zero, val_zero));

    // 1 = { 0 | }
    val_one = game_intern_cache_prep_and_get(game_from_game(val_zero, NULL));

    // ↑ = { 0 | * }
    val_up = game_intern_cache_prep_and_get(game_from_game(val_zero, val_star));

    // ↓ = { * | 0 }
    val_down = game_intern_cache_prep_and_get(game_from_game(val_star, val_zero));

    // ↑* = ↑ + * = { 0, * | 0 }
    Game **arr_zero = NULL;
    game_push(&arr_zero, val_zero);

    Game **arr_zero_star = NULL;
    game_push(&arr_zero_star, val_zero);
    game_push(&arr_zero_star, val_star);
    val_up_star = game_intern_cache_prep_and_get(game_from_games(arr_zero_star,
                                                                 arr_zero));

    // ↓* = ↓ + * = { 0 | 0, * }
    val_down_star = game_intern_cache_prep_and_get(game_from_games(arr_zero,
                                                                   arr_zero_star));

    game_free(&arr_zero);
    game_free(&arr_zero_star);
}

// -----------------------------------------------------------------
// Getters
// -----------------------------------------------------------------
Game* game_zero(void) { return val_zero; }
Game* game_star(void) { return val_star; }
Game* game_one(void)  { return val_one; }
Game* game_up(void)   { return val_up; }
Game* game_down(void) { return val_down; }
Game* game_up_star(void)   { return val_up_star; }
Game* game_down_star(void) { return val_down_star; }


int is_zero(Game *G) { return G == val_zero; }
int is_star(Game *G) { return G == val_star; }
int is_one(Game *G)  { return G == val_one; }

// These short games are not expected to be deeply nested
// relative to the solver limits, so recursion is acceptable here.
// -----------------------------------------------------------------
// Helper functions
// -----------------------------------------------------------------
// Returns 1 if the canonical game is a dyadic or integer number.
int is_number(Game *G) {
    if (!G) return 0;
    if (game_len(&G->left) == 0 && game_len(&G->right) == 0) return 1;
    if (game_len(&G->left) > 1 || game_len(&G->right) > 1) return 0;

    if (game_len(&G->left) == 1 && !is_number(G->left[0])) return 0;
    if (game_len(&G->right) == 1 && !is_number(G->right[0])) return 0;

    // Number condition: no GL >= GR may hold.
    // Without this check, for example * = {0|0} would be treated as a number.
    if (game_len(&G->left) == 1 && game_len(&G->right) == 1)
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
    if (game_len(&G->left) > 1 || game_len(&G->right) > 1) return 0;

    double l_val = 0.0, r_val = 0.0;
    int has_L = 0, has_R = 0;

    // A Left option, if present, must also be a number.
    if (game_len(&G->left) == 1) {
        if (!get_dyadic_value(G->left[0], &l_val)) return 0;
        has_L = 1;
    }
    // A Right option, if present, must also be a number.
    if (game_len(&G->right) == 1) {
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


int is_dyadic_plus_star(Game *G, double *out_dyadic_val) {
    if (game_len(&G->left) == 1 && game_len(&G->right) == 1 && G->left[0] == G->right[0]) {
        if(get_dyadic_value(G->left[0], out_dyadic_val)) return 1;
    }
    return 0;
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
        Game *prev = make_int(n - 1);
        Game **left = NULL;
        da_push(left, prev);

        Game *result = game_canonicalize(game_from_games(left, NULL));
        da_free(left);
        return result;
    } else {
        Game *prev = make_int(n + 1);
        Game **right = NULL;
        da_push(right, prev);

        Game *result = game_canonicalize(game_from_games(NULL, right));
        da_free(right);
        return result;
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

    Game **l_arr = NULL;
    Game **r_arr = NULL;
    da_push(l_arr, left);
    da_push(r_arr, right);

    Game *result = game_canonicalize(game_from_games(l_arr, r_arr));
    da_free(l_arr);
    da_free(r_arr);
    return result;
}


// -- make_nimber -----------------------------------------------------------
// Build nimber *n.  *0 = 0, *1 = *, *2 = {0,* | 0,*}, etc.
Game* make_nimber(int n) {
    if (n < 0) return NULL;
    if (n == 0) return game_zero();
    if (n == 1) return game_star();

    Game **opts = NULL;
    for (int i = 0; i < n; i++) {
        da_push(opts, make_nimber(i));
    }

    Game *result = game_canonicalize(game_from_games(opts, opts));
    da_free(opts);
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

    Game **new_left = NULL;
    Game **new_right = NULL;

    for (size_t i = 0; i < game_len(&G->right); i++) {
        Game *neg = game_negate(G->right[i]);
        if (!neg) {
            da_free(new_left);
            da_free(new_right);
            return NULL;
        }
        da_push(new_left, neg);
    }

    for (size_t i = 0; i < game_len(&G->left); i++) {
        Game *neg = game_negate(G->left[i]);
        if (!neg) {
            da_free(new_left);
            da_free(new_right);
            return NULL;
        }
        da_push(new_right, neg);
    }

    Game *res = game_canonicalize(game_from_games(new_left, new_right));
    da_free(new_left);
    da_free(new_right);
    return res;
}

typedef struct ProductMemoEntry {
    Game *left;
    Game *right;
    Game *product;
} ProductMemoEntry;

static Game *multiply_recursive(ProductMemoEntry **memo, Game *left, Game *right);

static Game *product_memo_get(ProductMemoEntry *memo,
                              Game *left,
                              Game *right) {
    for (size_t i = 0; i < da_len(memo); i++) {
        ProductMemoEntry *entry = &memo[i];
        if ((entry->left == left && entry->right == right) ||
            (entry->left == right && entry->right == left)) {
            return entry->product;
        }
    }
    return NULL;
}

static void product_memo_put(ProductMemoEntry **memo,
                             Game *left,
                             Game *right,
                             Game *product) {
    ProductMemoEntry entry = {
        .left = left,
        .right = right,
        .product = product
    };
    da_push(*memo, entry);
}

static Game *make_product_option(ProductMemoEntry **memo,
                                 Game *left,
                                 Game *right,
                                 Game *left_option,
                                 Game *right_option) {
    Game *first = multiply_recursive(memo, left_option, right);
    Game *second = multiply_recursive(memo, left, right_option);
    Game *overlap = multiply_recursive(memo, left_option, right_option);
    if (first == NULL || second == NULL || overlap == NULL) return NULL;

    Game *negative_overlap = game_negate(overlap);
    if (negative_overlap == NULL) return NULL;
    return game_add(game_add(first, second), negative_overlap);
}

static int append_product_options(ProductMemoEntry **memo,
                                  Game *left,
                                  Game *right,
                                  Game **left_options,
                                  Game **right_options,
                                  Game ***destination) {
    for (size_t i = 0; i < game_len(&left_options); i++) {
        for (size_t j = 0; j < game_len(&right_options); j++) {
            Game *option = make_product_option(memo, left, right,
                                               left_options[i],
                                               right_options[j]);
            if (option == NULL) return 0;
            game_push(destination, option);
        }
    }
    return 1;
}

static Game *multiply_recursive(ProductMemoEntry **memo,
                                Game *left,
                                Game *right) {
    if (left == NULL || right == NULL) return NULL;
    if (left == game_zero() || right == game_zero()) return game_zero();
    if (left == game_one()) return right;
    if (right == game_one()) return left;

    Game *cached = product_memo_get(*memo, left, right);
    if (cached != NULL) return cached;

    Game **product_left = NULL;
    Game **product_right = NULL;

    if (!append_product_options(memo, left, right,
                                left->left, right->left, &product_left) ||
        !append_product_options(memo, left, right,
                                left->right, right->right, &product_left) ||
        !append_product_options(memo, left, right,
                                left->left, right->right, &product_right) ||
        !append_product_options(memo, left, right,
                                left->right, right->left, &product_right)) {
        game_free(&product_left);
        game_free(&product_right);
        return NULL;
    }

    Game *product = game_canonicalize(
        game_from_games(product_left, product_right)
    );
    game_free(&product_left);
    game_free(&product_right);

    if (product != NULL) product_memo_put(memo, left, right, product);
    return product;
}

Game *game_multiply(Game *left, Game *right) {
    ProductMemoEntry *memo = NULL;
    Game *product = multiply_recursive(&memo, left, right);
    da_free(memo);
    return product;
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

    Game **new_left = NULL;
    Game **new_right = NULL;
    Game *star = game_star();

    game_foreach(left_game, &G->left) {
        da_push(new_left, game_add(cool_with_star(*left_game), star));
    }

    game_foreach(right_game, &G->right) {
        da_push(new_right, game_add(cool_with_star(*right_game), star));
    }

    Game *result = game_canonicalize(game_from_games(new_left, new_right));

    da_free(new_left);
    da_free(new_right);

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
    if (get_dyadic_value(H, &val)) {
        return H;
    }

    // H = x + *.
    if (is_dyadic_plus_star(H, &val)) {
        return game_add(H, game_star()); // x + * + * = x.
    }

    // General case: { p(H^L) | p(H^R) } over the canonical form.
    Game *H0 = game_canonicalize(H);

    Game **new_left = NULL;
    Game **new_right = NULL;

    for (size_t i = 0; i < game_len(&H0->left); i++) {
        da_push(new_left, star_projection(H0->left[i]));
    }

    for (size_t i = 0; i < game_len(&H0->right); i++) {
        da_push(new_right, star_projection(H0->right[i]));
    }

    Game *result = game_canonicalize(game_from_games(new_left, new_right));

    da_free(new_left);
    da_free(new_right);

    return result;
}
