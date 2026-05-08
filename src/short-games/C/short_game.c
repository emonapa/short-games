/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hackenbushe s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hackenbush Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "shared/error.h"
#include "shared/stack.h"
#include "shared/darray.h"
#include "shared/raw_game.h"

#include "config.h"
#include "memory.h"

#include "game_intern_cache.h"
#include "game_operations_cache.h"
#include "game_canon_cache.h"
#include "position_cache.h"

#include "short_game.h"
#include "singletons.h"

int cannon_count = 0;
int eq_count = 0;
int add_count = 0;
int make_count = 0;
int info_count = 100000000;

// debatable if needed, see da_free in the solve function
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

    size_t pos_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_POS) / sizeof(HashEntry));

    // Initialize all caches according to the configured memory budget.
    game_operations_cache_init(geq_size, add_size);
    game_canon_cache_init(canon_size);
    game_intern_cache_init(intern_size);
    position_cache_init(pos_size);

    singletons_init();
}

void short_game_free(void) {
    game_operations_cache_free_all();
    game_canon_cache_free();
    game_intern_cache_free();
    position_cache_free();
}

/* ------------------------------------------------------------
   Basic node construction
   ------------------------------------------------------------ */
Game* game_make(Game **left, int L_count, Game **right, int R_count) {
    Game *g = (Game*)malloc(sizeof(Game));
    if (g == NULL) error_exit(ERR_MALLOC, "");

    g->L_count = L_count;
    g->R_count = R_count;
    //TODO
    make_count++;
    if (make_count % info_count == 0) printf("[INFO] make count %d.   * %d\n", (int)(make_count/info_count), info_count);

    g->left = NULL;
    if (L_count > 0) {
        g->left = (Game**)malloc((size_t)L_count * sizeof(Game*));
        if (g->left == NULL) error_exit(ERR_MALLOC, "");

        for (int i = 0; i < L_count; i++) g->left[i] = left[i];
    }

    g->right = NULL;
    if (R_count > 0) {
        g->right = (Game**)malloc((size_t)R_count * sizeof(Game*));
        if (g->right == NULL) error_exit(ERR_MALLOC, "");

        for (int i = 0; i < R_count; i++) g->right[i] = right[i];
    }

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
    for (int i = 0; i < G->R_count; i++) {
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
    for (int i = 0; i < H->L_count; i++) {
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

static void replace_left_option(Game *G, int index, Game *GLR) {
    if (G == NULL || GLR == NULL) error_exit(ERR_NULL_POINTER, "");

    int old_count = G->L_count;
    int replace_count = GLR->L_count;
    int new_count = old_count - 1 + replace_count;

    Game **new_arr = NULL;
    if (new_count > 0) {
        new_arr = (Game**)malloc(new_count * sizeof(Game*));
        if (new_arr == NULL) error_exit(ERR_MALLOC, "");

        int dst = 0;
        // 1. Copy old options before the replaced index.
        for (int i = 0; i < index; i++) new_arr[dst++] = G->left[i];
        // 2. Insert grandchildren, which bypass the reversible option.
        for (int i = 0; i < replace_count; i++) new_arr[dst++] = GLR->left[i];
        // 3. Copy old options after the replaced index.
        for (int i = index + 1; i < old_count; i++) new_arr[dst++] = G->left[i];
    }

    if (G->left) free(G->left); // Free the original option array.
    G->left = new_arr;
    G->L_count = new_count;
}

static void replace_right_option(Game *G, int index, Game *GRL) {
    if (G == NULL || GRL == NULL) error_exit(ERR_NULL_POINTER, "");

    int old_count = G->R_count;
    int replace_count = GRL->R_count;
    int new_count = old_count - 1 + replace_count;

    Game **new_arr = NULL;
    if (new_count > 0) {
        new_arr = (Game**)malloc(new_count * sizeof(Game*));
        if(new_arr == NULL) error_exit(ERR_MALLOC, "");

        int dst = 0;
        for (int i = 0; i < index; i++) new_arr[dst++] = G->right[i];
        for (int i = 0; i < replace_count; i++) new_arr[dst++] = GRL->right[i];
        for (int i = index + 1; i < old_count; i++) new_arr[dst++] = G->right[i];
    }

    if (G->right) free(G->right);
    G->right = new_arr;
    G->R_count = new_count;
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
    if (G == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    if (G == game_zero() || G == game_star()) return G;

    Game *cached = NULL;
    if (game_canon_cache_get(G, &cached)) return cached;

    cannon_count++;

    // 1) Canonicalize children first, so GLR/GRL references are already clean interned pointers.
    // This is theoretically unnecessary in the standard solver, where every subgame is already
    // canonicalized. It is still needed for other callers, for example the calculator.
    for (int i = 0; i < G->L_count; i++) G->left[i] = game_canonicalize(G->left[i]);
    for (int i = 0; i < G->R_count; i++) G->right[i] = game_canonicalize(G->right[i]);

    int changed = 1;
    while (changed) {
        changed = 0;

        // 2) Left reversible options.
        for (int i = 0; i < G->L_count; i++) {
            Game *GL = G->left[i];
            int reversed = 0;

            for (int j = 0; j < GL->R_count; j++) {
                Game *GLR = GL->right[j];
                // Test whether Right has a reply that is no worse than the original game (GLR <= G).
                if (game_geq(G, GLR)) {
                    replace_left_option(G, i, GLR); // Remove the reversible option and insert its Left options.
                    reversed = 1;
                    break;
                }
            }
            if (reversed) {
                changed = 1;
                break; // The option array changed, so restart the scan.
            }
        }
        if (changed) continue;

        // 3) Right reversible options.
        for (int i = 0; i < G->R_count; i++) {
            Game *GR = G->right[i];
            int reversed = 0;

            for (int j = 0; j < GR->L_count; j++) {
                Game *GRL = GR->left[j];
                // Test whether Left has a reply that is no worse than the original game (GRL >= G).
                if (game_geq(GRL, G)) {
                    replace_right_option(G, i, GRL); // Remove the reversible option and insert its Right options.
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
        int keep_L = 0;
        for (int i = 0; i < G->L_count; i++) {
            Game *cand = G->left[i];
            int dominated = 0;

            for (int j = 0; j < keep_L; j++) {
                Game *kept = G->left[j];
                if (kept == cand || game_geq(kept, cand)) {
                    dominated = 1;
                    break;
                }
                if (game_geq(cand, kept)) {
                    G->left[j] = G->left[--keep_L];
                    j--;
                }
            }
            if (!dominated) G->left[keep_L++] = cand;
        }
        if (keep_L != G->L_count) {
            G->L_count = keep_L;
            changed = 1;
        }
        if (changed) continue;

        // 5) Right dominated options.
        int keep_R = 0;
        for (int i = 0; i < G->R_count; i++) {
            Game *cand = G->right[i];
            int dominated = 0;

            for (int j = 0; j < keep_R; j++) {
                Game *kept = G->right[j];
                if (kept == cand || game_geq(cand, kept)) {
                    dominated = 1;
                    break;
                }
                if (game_geq(kept, cand)) {
                    G->right[j] = G->right[--keep_R];
                    j--;
                }
            }
            if (!dominated) G->right[keep_R++] = cand;
        }
        if (keep_R != G->R_count) {
            G->R_count = keep_R;
            changed = 1;
        }
    }

    // 6) Intern the canonical node.
    game_intern_cache_prepare(G);
    Game *I = game_intern_cache_get(G);

    game_canon_cache_put(G, I);
    return I;
}


/* ------------------------------------------------------------
   Game sum with memoization
   ------------------------------------------------------------ */
Game* game_add(Game *G, Game *H) {
    if (G == NULL && H == NULL) error_exit(ERR_NULL_POINTER, "Both games to add are NULL.\n");
    if (!G) return H;
    if (!H) return G;

    if (G->L_count == 0 && G->R_count == 0) return H;
    if (H->L_count == 0 && H->R_count == 0) return G;

    Game *memo = NULL;
    if (game_add_cache_get(G, H, &memo)) return memo;

    //TODO
    add_count++;
    if (add_count % info_count == 0) printf("[INFO] add count %d.   *%d\n", (int)(add_count/info_count), info_count);

    int new_l_count = G->L_count + H->L_count;
    int new_r_count = G->R_count + H->R_count;

    Game **left_opts = NULL;
    Game **right_opts = NULL;
    if (new_l_count > 0) {
        left_opts = (Game**)malloc((size_t)new_l_count * sizeof(Game*));
        if (left_opts == NULL) error_exit(ERR_MALLOC, "");
    }
    if (new_r_count > 0) {
        right_opts = (Game**)malloc((size_t)new_r_count * sizeof(Game*));
        if (right_opts == NULL) error_exit(ERR_MALLOC, "");
    }

    int idx = 0;
    for (int i = 0; i < G->L_count; i++) left_opts[idx++] = game_add(G->left[i], H);
    for (int i = 0; i < H->L_count; i++) left_opts[idx++] = game_add(G, H->left[i]);

    idx = 0;
    for (int i = 0; i < G->R_count; i++) right_opts[idx++] = game_add(G->right[i], H);
    for (int i = 0; i < H->R_count; i++) right_opts[idx++] = game_add(G, H->right[i]);

    Game *sum = game_make(left_opts, new_l_count, right_opts, new_r_count);
    sum = game_canonicalize(sum);
    if (sum == NULL) warning("Got NULL from canonization.\n");

    if (left_opts) free(left_opts);
    if (right_opts) free(right_opts);

    game_add_cache_put(G, H, sum);
    return sum;
}


Game* solve_component(RawGame_t raw_game, Position_t position) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");

    // Memoization for already solved raw positions.
    Game *memo = NULL;
    if (position_cache_get(raw_game, position, &memo))
        return memo;

    Game **left_opts = NULL;
    Game **right_opts = NULL;
    int l_count = 0, r_count = 0;

    // Recursively evaluate all legal moves from this position.
    for (int e = 0; e < num_moves(raw_game); ++e) {
        Game *child = NULL;
        if (can_left_move(raw_game, position, e)) {
            Position_t child_position = do_move_left(raw_game, position, e);
            if (child_position == NULL) error_exit(ERR_MALLOC, "");

            child = solve_component(raw_game, child_position);
            da_push(left_opts, child);
        }
        if (can_right_move(raw_game, position, e)) {
            Position_t child_position = do_move_right(raw_game, position, e);
            if (child_position == NULL) error_exit(ERR_MALLOC, "");

            child = solve_component(raw_game, child_position);
            da_push(right_opts, child);
        }
    }

    Game *G = game_canonicalize(game_make(left_opts, (int)da_len(left_opts),
                                          right_opts, (int)da_len(right_opts)));

    da_free(left_opts);
    da_free(right_opts);
    position_cache_insert(raw_game, position, G);
    return G;
}

static void print_stats() {
    printf("[CACHE] canon_count     = %ld\n", canon_items_count);
    printf("[CACHE] intern_count    = %ld\n", intern_items_count);
    printf("[CACHE] add_count       = %ld\n", add_items_count);
    printf("[CACHE] geq_count       = %ld\n", geq_items_count);
    printf("[CACHE] pos_items_count = %ld\n", pos_items_count);
    printf("[META] make_count       = %d\n", make_count);
}

//#define PRINT_RESULT

Game* solve(void *raw_game, void *position) {
    Position_t *sub_games = NULL;
    int count = get_independent_components(raw_game, position, &sub_games);

    Game *total_sum = game_zero();


#ifdef PRINT_RESULT
    printf("======================END RESULT==========================\n");
#endif
    // Solve each independent component and add it to the accumulated game value.
    for (int i = 0; i < count; i++) {
        Game *sub_game = solve_component(raw_game, sub_games[i]);
        total_sum = game_add(total_sum, sub_game);
#ifdef PRINT_RESULT
       printf("-------------[%d] Průchod-------------\n", i);
       print_stats();
#endif
       // Components are usually different, so the cache could be reset here,
       // but hints and educational mode would become extremely slow if this were enabled.
       //solver_free();
       //solver_initialize(g_memory_multiplier);
    }
#ifdef PRINT_RESULT
    printf("=========================================================\n");
#endif

    game_canonicalize(total_sum);

#ifdef PRINT_RESULT
    const char *game_string = game_get_string(total_sum, FORMAT_FORMATED);
    printf("Result: %s", game_string);
#endif

    for (int i = 0; i < count; i++) free(sub_games[i]);
    // This freeing is possible here, but it makes the code less general, so just leak it!
    //da_free(sub_games);

    return total_sum;
}
