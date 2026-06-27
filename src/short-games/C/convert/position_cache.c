#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "../shared/error.h"
#include "../config.h"

// TODO
#include "../core/singletons.h"
#include "../core/short_game.h"
//

#include "position_cache.h"
#include "convert_interface/raw_game.h"

static size_t pos_memo_size = 0;
static size_t pos_memo_mask = 0;
size_t pos_items_count = 0;
static size_t pos_max_items = 0;
static HashEntry *position_cache = NULL;

static uint64_t hash_graph_state(void *raw_game, void *position) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");
    uint64_t total_hash = 0;

    for (int e = 0; e < num_moves(raw_game); e++) {
        if (can_left_move(raw_game, position, e) ||
            can_right_move(raw_game, position, e)) {
                total_hash ^= hash_raw_game_position(raw_game, position, e);
        }
    }
    return total_hash;
}

void position_cache_init(size_t pos_size) {
    if (pos_size <= 0)
        error_exit(ERR_SOLVE_WITH_NONPOSITIVE_MEM, "Trying to initialize position cache with size %zu.\n", pos_size);
    if (position_cache == NULL) {
        pos_memo_size = pos_size;
        pos_memo_mask = pos_size - 1;
        pos_max_items = MAX_ITEMS(pos_size);

        position_cache = (HashEntry *)calloc(pos_memo_size, sizeof(HashEntry));
        if (position_cache == NULL) error_exit(ERR_MALLOC, "");
    }
}

void position_cache_free(void) {
    free(position_cache);
    position_cache = NULL;
}

int position_cache_get(void *raw_game, void *position, Game **out_value) {
    if (raw_game == NULL || position == NULL || out_value == NULL) error_exit(ERR_NULL_POINTER, "");

    uint64_t h = hash_graph_state(raw_game, position);
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

void position_cache_insert(void *raw_game, void *position, Game *value) {
    if (raw_game == NULL || position == NULL || value == NULL) error_exit(ERR_NULL_POINTER, "");

    static int already_reported = 0;
    if (pos_items_count >= pos_max_items) {
        if (!already_reported) {
            warning("Position cache full at %zu items, no new elements.\n", pos_items_count);
            already_reported = 1;
        }
        return;
    }

    uint64_t h = hash_graph_state(raw_game, position);
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
