#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "../shared/error.h"
#include "../config.h"

#include "game_operations_cache.h"

// GEQ cache
static size_t geq_memo_size = 0;
static size_t geq_memo_mask = 0;
size_t geq_items_count = 0;
static size_t geq_max_items = 0;
static GeqEntry *game_geq_cache = NULL;

// ADD cache
static size_t add_memo_size = 0;
static size_t add_memo_mask = 0;
size_t add_items_count = 0;
static size_t add_max_items = 0;
static AddEntry *game_add_cache = NULL;

static uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static uint32_t hash_pair(uintptr_t a, uintptr_t b, uint32_t mask) {
    uint64_t x = (uint64_t)a;
    uint64_t y = (uint64_t)b;
    uint64_t h = mix64(x ^ (y + 0x9e3779b97f4a7c15ULL + (x << 6) + (x >> 2)));
    return (uint32_t)(h & mask);
}


void game_operations_cache_init(size_t geq_size, size_t add_size) {
    if (geq_size <= 0 || add_size <= 0)
        error_exit(ERR_SOLVE_WITH_NONPOSITIVE_MEM, "Trying to initialize geq and add cache sizes %zu and %zu.\n", geq_size, add_size);

    if (game_geq_cache == NULL) {
        geq_memo_size = geq_size;
        geq_memo_mask = geq_size - 1;
        geq_max_items = MAX_ITEMS(geq_size);

        game_geq_cache = (GeqEntry *)calloc(geq_memo_size, sizeof(GeqEntry));
        if (game_geq_cache == NULL) error_exit(ERR_MALLOC, "");
    }

    if (game_add_cache == NULL) {
        add_memo_size = add_size;
        add_memo_mask = add_size - 1;
        add_max_items = MAX_ITEMS(add_size);

        game_add_cache = (AddEntry *)calloc(add_memo_size, sizeof(AddEntry));
        if (game_add_cache == NULL) error_exit(ERR_MALLOC, "");
    }
}

void game_operations_cache_free_all(void) {
    free(game_geq_cache);
    free(game_add_cache);
    game_geq_cache = NULL;
    game_add_cache = NULL;
}

static void normalize_pair(uintptr_t *a, uintptr_t *b) {
    if (*a > *b) {
        uintptr_t tmp = *a;
        *a = *b;
        *b = tmp;
    }
}

// add memo
int game_add_cache_get(Game *A, Game *B, Game **out) {
    if (A == NULL || B == NULL) error_exit(ERR_NULL_POINTER, "");

    uintptr_t a = (uintptr_t)A;
    uintptr_t b = (uintptr_t)B;
    normalize_pair(&a, &b);

    size_t idx = hash_pair(a, b, add_memo_mask);

    for (size_t i = 0; i < PROBE_LIMIT; i++) {
        size_t j = (idx + i) & add_memo_mask;
        if (!game_add_cache[j].used) return 0;
        if (game_add_cache[j].a == a && game_add_cache[j].b == b) {
            *out = game_add_cache[j].value;
            return 1;
        }
    }
    return 0;
}

void game_add_cache_put(Game *A, Game *B, Game *value) {
    if (A == NULL || B == NULL) error_exit(ERR_NULL_POINTER, "");

    static int already_reported = 0;
    if (add_items_count >= add_max_items) {
        if (!already_reported) {
            warning("Add cache full at %zu items, no new elements added.\n", add_items_count);
            already_reported = 1;
        }
        return;
    }

    uintptr_t a = (uintptr_t)A;
    uintptr_t b = (uintptr_t)B;
    normalize_pair(&a, &b);

    size_t idx = hash_pair(a, b, add_memo_mask);

    for (size_t i = 0; i < PROBE_LIMIT; i++) {
        size_t j = (idx + i) & add_memo_mask;

        if (game_add_cache[j].used && game_add_cache[j].a == a && game_add_cache[j].b == b) {
            game_add_cache[j].value = value;
            return;
        }

        if (!game_add_cache[j].used) {
            game_add_cache[j].used = 1;
            game_add_cache[j].a = a;
            game_add_cache[j].b = b;
            game_add_cache[j].value = value;

            add_items_count++;
            return;
        }
    }
}

// geq memo
int game_geq_cache_get(Game *A, Game *B, uint8_t *out) {
    uintptr_t a = (uintptr_t)A;
    uintptr_t b = (uintptr_t)B;
    size_t idx = hash_pair(a, b, geq_memo_mask);

    for (size_t i = 0; i < PROBE_LIMIT; i++) {
        size_t j = (idx + i) & geq_memo_mask;
        if (!game_geq_cache[j].used) return 0;
        if (game_geq_cache[j].a == a && game_geq_cache[j].b == b) {
            *out = game_geq_cache[j].value;
            return 1;
        }
    }
    return 0;
}

void game_geq_cache_put(Game *A, Game *B, uint8_t value) {
    if (A == NULL || B == NULL) error_exit(ERR_NULL_POINTER, "");

    static int already_reported = 0;
    if (geq_items_count >= geq_max_items) {
        if (!already_reported) {
            warning("Geq cache full at %zu items, no new elements.\n", geq_items_count);
            already_reported = 1;
        }
        return;
    }

    uintptr_t a = (uintptr_t)A;
    uintptr_t b = (uintptr_t)B;
    size_t idx = hash_pair(a, b, geq_memo_mask);

    for (size_t i = 0; i < PROBE_LIMIT; i++) {
        size_t j = (idx + i) & geq_memo_mask;

        if (game_geq_cache[j].used && game_geq_cache[j].a == a && game_geq_cache[j].b == b) {
            game_geq_cache[j].value = value;
            return;
        }

        if (!game_geq_cache[j].used) {
            game_geq_cache[j].used = 1;
            game_geq_cache[j].a = a;
            game_geq_cache[j].b = b;
            game_geq_cache[j].value = (uint8_t)(value ? 1 : 0);

            geq_items_count++;
            return;
        }
    }
}
