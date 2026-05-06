/*
caching game
    ({L1, L2, ... | R1, R2, ...})
    ->
    (*G)

    používá se protože když vyrobíme v nějakém vypočtu stejnou hru,
    tak ať používáme pro stejnou strukturu jeden pointer a
    nemusíme mít ve výpočtech větší overhead.
    Bez této cache nebude fungovat většina výpočtu, například
    porovnávání hry pomocí pointeru.
*/

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "shared/error.h"
#include "config.h"

#include "game_intern_cache.h"

// INTERN cache
static size_t intern_memo_size = 0;
static size_t intern_memo_mask = 0;
size_t intern_items_count = 0;
static size_t intern_max_items = 0;
static InternEntry *game_intern_cache = NULL;

static int cmp_game_ptr(const void *a, const void *b) {
    uintptr_t x = (uintptr_t)(*(const Game * const *)a);
    uintptr_t y = (uintptr_t)(*(const Game * const *)b);
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

void game_intern_cache_prepare(Game *G) {
    if (G == NULL) error_exit(ERR_NULL_POINTER, "");

    // {L1, L2 | R1, R2} = {L2, L1 | R2, R1}
    if (G->L_count > 1) qsort(G->left,  (size_t)G->L_count, sizeof(Game*), cmp_game_ptr);
    if (G->R_count > 1) qsort(G->right, (size_t)G->R_count, sizeof(Game*), cmp_game_ptr);
}

static uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static uint64_t hash_node(Game *G) {
    uint64_t h = 1469598103934665603ULL;

    h ^= (uint64_t)G->L_count; h *= 1099511628211ULL;
    h ^= (uint64_t)G->R_count; h *= 1099511628211ULL;

    for (int i = 0; i < G->L_count; i++) {
        h ^= mix64((uint64_t)(uintptr_t)G->left[i]);
        h *= 1099511628211ULL;
    }
    for (int i = 0; i < G->R_count; i++) {
        h ^= mix64((uint64_t)(uintptr_t)G->right[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

static int node_equal(Game *A, Game *B) {
    if (A->L_count != B->L_count) return 0;
    if (A->R_count != B->R_count) return 0;
    for (int i = 0; i < A->L_count; i++) if (A->left[i] != B->left[i]) return 0;
    for (int i = 0; i < A->R_count; i++) if (A->right[i] != B->right[i]) return 0;
    return 1;
}

void game_intern_cache_init(size_t intern_size) {
    if (intern_size == 0)
        error_exit(ERR_SOLVE_WITH_NONPOSITIVE_MEM, "Trying to initialize intern cache with size %zu.\n", intern_size);

    if (game_intern_cache == NULL) {
        intern_memo_size = intern_size;
        intern_memo_mask = intern_size - 1;
        // Limit dáme na 95 %, protože vypnutí internu je fatální
        intern_max_items = (size_t)(intern_size * 0.95);

        game_intern_cache = (InternEntry *)calloc(intern_memo_size, sizeof(InternEntry));
        if (game_intern_cache == NULL) error_exit(ERR_MALLOC, "");
    }
}

void game_intern_cache_free(void) {
    free(game_intern_cache);
    game_intern_cache = NULL;
}

Game* game_intern_cache_get(Game *G) {
    if (G == NULL) error_exit(ERR_NULL_POINTER, "");

    size_t idx = (size_t)(hash_node(G) & intern_memo_mask);

    for (size_t i = 0; i < PROBE_LIMIT; i++) {
        size_t j = (idx + i) & intern_memo_mask;

        if (!game_intern_cache[j].used) {
            game_intern_cache[j].used = 1;
            game_intern_cache[j].node = G;

            intern_items_count++;

            static int already_reported = 0;
            if (intern_items_count >= intern_max_items) {
                if (!already_reported) {
                    warning("[FATAL] Intern cache full at %zu items.\n", intern_items_count);
                    warning("        Calculations are now most likely wrong.\n");
                    already_reported = 1;
                }
            }

            return G;
        }

        if (node_equal(game_intern_cache[j].node, G)) {
            return game_intern_cache[j].node;
        }
    }

    // game_intern_cacheulka plna, fallback
    return G;
}
