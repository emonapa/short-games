#ifndef POSITION_CACHE_H
#define POSITION_CACHE_H
#include <stdint.h>
#include "raw_game.h"
#include "short_game.h"

typedef struct { edge_mask_t live_mask; } PositionKey;

typedef struct {
    uint8_t  used;
    uint64_t hash;
    Game* value;
} HashEntry;

void position_cache_init(size_t pos_size);
void position_cache_free(void);

int position_cache_get(const BaseGraph *g, edge_mask_t live_mask, Game **out_value);
void position_cache_insert(const BaseGraph *g, edge_mask_t live_mask, Game *value);

#endif // POSITION_CACHE_H
