#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "position_cache.h"
#include "config.h"
#include "singletons.h"

static size_t pos_memo_size = 0;
static size_t pos_memo_mask = 0;
size_t pos_items_count = 0;
static size_t pos_max_items = 0;
static HashEntry *position_cache = NULL;

static uint64_t hash_graph_state(const BaseGraph *g, edge_mask_t live_mask) {
    uint64_t total_hash = 0;
    uint64_t h = 14695981039346656037ULL;

    for (int e = 0; e < g->num_edges; e++) {
        if (live_mask & BIT(e)) {
            h ^= g->edges[e].u;     h *= 1099511628211ULL;
            h ^= g->edges[e].v;     h *= 1099511628211ULL;
            h ^= g->edges[e].color; h *= 1099511628211ULL;
            total_hash ^= h;
        }
    }
    return total_hash;
}

void position_cache_init(size_t pos_size) {
    if (position_cache == NULL) {
        pos_memo_size = pos_size;
        pos_memo_mask = pos_size - 1;
        printf("pos_mask = %lX\n", pos_memo_mask);
        pos_max_items = (size_t)(pos_size * 0.75);

        position_cache = (HashEntry *)calloc(pos_memo_size, sizeof(HashEntry));
        if (position_cache == NULL) {
            fprintf(stderr, "FATAL ERROR: Nedostatek pameti pro POSITION cache!\n");
            exit(EXIT_FAILURE);
        }
    }
}

void position_cache_free(void) {
    free(position_cache);
    position_cache = NULL;
}

int position_cache_get(const BaseGraph *g, edge_mask_t live_mask, Game **out_value) {
    if (live_mask == 0) return 0;

    assert(g != NULL);
    uint64_t h = hash_graph_state(g, live_mask);
    size_t idx = (size_t)(h & pos_memo_mask);

    for (size_t i = 0; i < PROBE_LIMIT; ++i) {
        size_t j = (idx + i) & pos_memo_mask;
        if (!position_cache[j].used) {
            return 0; // nenalezeno
        }

        if (position_cache[j].hash == h) {
            if (out_value) *out_value = position_cache[j].value;
            return 1;
        }
    }
    return 0;
}

void position_cache_insert(const BaseGraph *g, edge_mask_t live_mask, Game *value) {
    assert(g != NULL);
    if (live_mask == 0) return;

    uint64_t h = hash_graph_state(g, live_mask);
    size_t idx = (size_t)(h & pos_memo_mask);

    for (size_t i = 0; i < PROBE_LIMIT; ++i) {
        size_t j = (idx + i) & pos_memo_mask;

        if (position_cache[j].used && position_cache[j].hash == h) {
            position_cache[j].value = value;
            return;
        }

        if (!position_cache[j].used) {
            position_cache[j].used = 1;
            position_cache[j].hash = h; // Uložíme si hash
            position_cache[j].value = value;
            pos_items_count++;
            return;
        }
    }
}
