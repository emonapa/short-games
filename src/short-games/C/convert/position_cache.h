#ifndef POSITION_CACHE_H
#define POSITION_CACHE_H
#include <stdint.h>

#include "../core/short_game.h"
#include "convert_interface/raw_game.h"

typedef struct {
    uint8_t  used;
    uint64_t hash;
    Game* value;
} HashEntry;

void position_cache_init(size_t pos_size);
void position_cache_free(void);

int position_cache_get(RawGame_t raw_game, Position_t position, Game **out_value);
void position_cache_insert(RawGame_t raw_game, Position_t position, Game *value);

#endif // POSITION_CACHE_H
