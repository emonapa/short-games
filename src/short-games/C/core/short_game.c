/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "../shared/error.h"
#include "../shared/stack.h"
#include "../shared/memory.h"
#include "../config.h"

#include "game_intern_cache.h"
#include "game_operations_cache.h"
#include "game_canon_cache.h"

#include "short_game.h"
#include "singletons.h"
#include "game_darray.h"

int cannon_count = 0;
int eq_count = 0;
int add_count = 0;
int make_count = 0;
int info_count = 100000000;

// debatable if needed, see game_free in the solve function
//float g_memory_multiplier = 0.5;

void short_game_init(float memory_multiplier) {
    if (memory_multiplier > 1 || memory_multiplier <= 0)
        error_exit(ERR_OTHER, "%f is invalid fraction argument.\n", memory_multiplier);

    //g_memory_multiplier = memory_multiplier;
    size_t free_ram_bytes = get_size_free_memory();

    size_t ram_to_use = free_ram_bytes * memory_multiplier;

    size_t geq_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_GEQ) / sizeof(GeqEntry));
    size_t add_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_ADD) / sizeof(AddEntry));

    size_t canon_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_CANON) / sizeof(CanonEntry));
    size_t intern_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_INTERN) / sizeof(InternEntry));

    //size_t pos_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_POS) / sizeof(HashEntry));

    // Initialize all caches according to the configured memory budget.
    game_operations_cache_init(geq_size, add_size);
    game_canon_cache_init(canon_size);
    game_intern_cache_init(intern_size);
    //position_cache_init(pos_size);

    singletons_init();
}

void short_game_free(void) {
    game_operations_cache_free_all();
    game_canon_cache_free();
    game_intern_cache_free();
    //position_cache_free();
}

/* ------------------------------------------------------------
   Basic node construction
   ------------------------------------------------------------ */

Game* game_new() {
    Game *g = (Game*)malloc(sizeof(Game));
    if (g == NULL) error_exit(ERR_MALLOC, "New Game can't be created.\n");
    g->left = NULL;
    g->right = NULL;

    if (make_count % info_count == 0) {
        printf("[INFO] make count %d.   * %d\n",
               (int)(make_count / info_count), info_count);
    }
    make_count++;

    return g;
}

Game* game_from_games(Game **left, Game **right) {
    Game *g = game_new();

    if (left != NULL) game_append_many(&g->left, left);
    if (right != NULL) game_append_many(&g->right, right);

    return g;
}

Game *game_from_game(Game *left, Game *right) {
    Game *g = game_new();

    if (left != NULL) game_append(&g->left, left);
    if (right != NULL) game_append(&g->right, right);

    return g;
}



int game_geq(Game *G, Game *H) {
    if (G == NULL || H == NULL) error_exit(ERR_NULL_POINTER, "");

    if (G == H) return 1;

    uint8_t memo;
    if (game_geq_cache_get(G, H, &memo)) return (int)memo;

    // First part of the Conway definition:
    // For each right option G^R, the following must hold:
    //     !(H >= G^R)
    //
    // If there is a right option G^R such that H >= G^R,
    // then G >= H does not hold.
    for (size_t i = 0; i < game_len(&G->right); i++) {
        Game *GR = G->right[i];

        if (game_geq(H, GR)) {
            game_geq_cache_put(G, H, 0);
            return 0;
        }
    }

    // Second part of the Conway definition:
    // For each left option H^L, the following must hold:
    //     !(H^L >= G)
    //
    // If there is a left option H^L such that H^L >= G,
    // then G >= H does not hold.
    for (size_t i = 0; i < game_len(&H->left); i++) {
        Game *HL = H->left[i];

        if (game_geq(HL, G)) {
            game_geq_cache_put(G, H, 0);
            return 0;
        }
    }

    // If there is no problematic right option of G and no problematic
    // left option of H, then G >= H.
    game_geq_cache_put(G, H, 1);
    return 1;
}

int game_eq(Game *G, Game *H) {
    return game_geq(G, H) && game_geq(H, G);
}



Game* game_remove_dom_and_rev(Game *G) {
    int changed = 1;
    while (changed) {
        changed = 0;

        // 2) Left reversible options.
        for (size_t i = 0; i < game_len(&G->left); i++) {
            Game *GL = G->left[i];
            int reversed = 0;

            for (size_t j = 0; j < game_len(&GL->right); j++) {
                Game *GLR = GL->right[j];
                // Test whether Right has a reply that is no worse than the original game (GLR <= G).
                if (game_geq(G, GLR)) {
                    game_append_many(&G->left, GLR->left);
                    game_remove_unordered(&G->left, i);
                    reversed = 1;
                    break;
                }
            }

            if (reversed) {
                changed = 1;
                break;
            }
        }
        if (changed) continue;

        // 3) Right reversible options.
        for (size_t i = 0; i < game_len(&G->right); i++) {
            Game *GR = G->right[i];
            int reversed = 0;

            for (size_t j = 0; j < game_len(&GR->left); j++) {
                Game *GRL = GR->left[j];
                // Test whether Left has a reply that is no worse than the original game (GRL >= G).
                if (game_geq(GRL, G)) {
                    game_append_many(&G->right, GRL->right);
                    game_remove_unordered(&G->right, i);
                    reversed = 1;
                    break;
                }
            }

            if (reversed) {
                changed = 1;
                break;
            }
        }
        if (changed) continue;

        // 4) Left dominated options.
        for (size_t i = 0; i < game_len(&G->left); i++) {
            Game *cand = G->left[i];
            int dominated = 0;

            for (size_t j = 0; j < game_len(&G->left); j++) {
                if (i == j) continue;

                Game *other = G->left[j];
                if (other == cand || game_geq(other, cand)) {
                    dominated = 1;
                    break;
                }
            }

            if (dominated) {
                game_remove_unordered(&G->left, i);
                changed = 1;
                break;
            }
        }
        if (changed) continue;

        // 5) Right dominated options.
        for (size_t i = 0; i < game_len(&G->right); i++) {
            Game *cand = G->right[i];
            int dominated = 0;

            for (size_t j = 0; j < game_len(&G->right); j++) {
                if (i == j) continue;

                Game *other = G->right[j];
                if (other == cand || game_geq(cand, other)) {
                    dominated = 1;
                    break;
                }
            }

            if (dominated) {
                game_remove_unordered(&G->right, i);
                changed = 1;
                break;
            }
        }
    }

    // 6) Intern the canonical node.
    game_intern_cache_prepare(G);
    Game *I = game_intern_cache_get(G);

    game_canon_cache_put(G, I);
    return I;
}


// -----------------------------------------------------------------
// MAIN CANONICALIZATION FUNCTION
// -----------------------------------------------------------------
// 1. Remove left reversible options.
// 2. Remove right reversible options.
// 3. Remove left dominated options.
// 4. Remove right dominated options.
// 5. Intern the resulting canonical node.
Game* game_canonicalize(Game *G) {
    if (G == NULL) error_exit(ERR_NULL_POINTER, "");

    if (G == game_zero() || G == game_star()) return G;

    Game *cached = NULL;
    if (game_canon_cache_get(G, &cached)) return cached;
    cannon_count++;

    // 1) Canonicalize children first, so GLR/GRL references are already clean interned pointers.
    // This is theoretically unnecessary in the standard solver, where every subgame is already
    // canonicalized. It is still needed for other callers, for example the calculator.
    game_foreach(it, &G->left) *it = game_canonicalize(*it);
    game_foreach(it, &G->right) *it = game_canonicalize(*it);

    return game_remove_dom_and_rev(G);
}

Game* game_canonicalize_shallow(Game *G) {
    if (G == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    if (G == game_zero() || G == game_star()) return G;

    Game *cached = NULL;
    if (game_canon_cache_get(G, &cached)) return cached;
    cannon_count++;

    return game_remove_dom_and_rev(G);
}




/* ------------------------------------------------------------
   Game sum with memoization
   ------------------------------------------------------------ */
Game* game_add(Game *G, Game *H) {
    if (G == NULL && H == NULL) error_exit(ERR_NULL_POINTER, "Both games to add are NULL.\n");
    if (!G) return H;
    if (!H) return G;

    if (game_len(&G->left) == 0 && game_len(&G->right) == 0) return H;
    if (game_len(&H->left) == 0 && game_len(&H->right) == 0) return G;

    Game *memo = NULL;
    if (game_add_cache_get(G, H, &memo)) return memo;

    add_count++;
    if (add_count % info_count == 0) {
        printf("[INFO] add count %d.   *%d\n",
               (int)(add_count / info_count), info_count);
    }

    Game **left_opts = NULL;
    Game **right_opts = NULL;

    game_reserve(&left_opts, game_len(&G->left) + game_len(&H->left));
    game_reserve(&right_opts, game_len(&G->right) + game_len(&H->right));

    for (size_t i = 0; i < game_len(&G->left); i++) {
        game_push(&left_opts, game_add(G->left[i], H));
    }
    for (size_t i = 0; i < game_len(&H->left); i++) {
        game_push(&left_opts, game_add(G, H->left[i]));
    }

    for (size_t i = 0; i < game_len(&G->right); i++) {
        game_push(&right_opts, game_add(G->right[i], H));
    }
    for (size_t i = 0; i < game_len(&H->right); i++) {
        game_push(&right_opts, game_add(G, H->right[i]));
    }

    Game *sum = game_canonicalize(game_from_games(left_opts, right_opts));
    if (sum == NULL) warning("Got NULL from canonization.\n");

    game_free(&left_opts);
    game_free(&right_opts);

    game_add_cache_put(G, H, sum);
    return sum;
}
