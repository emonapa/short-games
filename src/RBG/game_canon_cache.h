#ifndef HASH_ONE_GAME_H
#define HASH_ONE_GAME_H

#include <stdint.h>
#include "short_game.h"

typedef struct {
    uint8_t used;
    uintptr_t key;
    Game *value;
} CanonEntry;


void game_canon_cache_init(size_t canon_size);
void game_canon_cache_free(void);

int  game_canon_cache_get(Game *key, Game **out);
void game_canon_cache_put(Game *key, Game *value);

#endif
