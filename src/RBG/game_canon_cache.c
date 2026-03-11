#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include "game_canon_cache.h"
#include "config.h"

static size_t canon_memo_size = 0;
static size_t canon_memo_mask = 0;
size_t canon_items_count = 0;
static size_t canon_max_items = 0;
static CanonEntry *game_canon_cache = NULL;

static uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static uint32_t hash_ptr(uintptr_t p) {
    return (uint32_t)(mix64((uint64_t)p) & canon_memo_mask);
}

void game_canon_cache_init(size_t canon_size) {
    if (game_canon_cache == NULL) {
        canon_memo_size = canon_size;
        canon_memo_mask = canon_size - 1;
        canon_max_items = (size_t)(canon_size * 0.75); // 75 % limit

        game_canon_cache = (CanonEntry *)calloc(canon_memo_size, sizeof(CanonEntry));
        if (game_canon_cache == NULL) {
            fprintf(stderr, "FATAL ERROR: Nedostatek pameti pro CANON cache!\n");
            exit(EXIT_FAILURE);
        }
    }
}

void game_canon_cache_free(void) {
    free(game_canon_cache);
    game_canon_cache = NULL;
}

int game_canon_cache_get(Game *key, Game **out) {
    assert(key != NULL);
    uintptr_t k = (uintptr_t)key;
    size_t idx = hash_ptr(k);

    for (size_t i = 0; i < PROBE_LIMIT; i++) {
        size_t j = (idx + i) & canon_memo_mask;
        if (!game_canon_cache[j].used) return 0;
        if (game_canon_cache[j].key == k) {
            *out = game_canon_cache[j].value;
            return 1;
        }
    }
    return 0;
}

void game_canon_cache_put(Game *key, Game *value) {
    assert(key != NULL);
    uintptr_t k = (uintptr_t)key;
    size_t idx = hash_ptr(k);

    for (size_t i = 0; i < PROBE_LIMIT; i++) {
        size_t j = (idx + i) & canon_memo_mask;

        if (game_canon_cache[j].used && game_canon_cache[j].key == k) {
            game_canon_cache[j].value = value;
            return;
        }

        if (!game_canon_cache[j].used) {
            game_canon_cache[j].used = 1;
            game_canon_cache[j].key = k;
            game_canon_cache[j].value = value;

            canon_items_count++;
            return;
        }
    }
}
