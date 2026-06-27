#ifndef GAME_INTERN_CACHE_H
#define GAME_INTERN_CACHE_H

#include <stdint.h>
#include "short_game.h"

typedef struct {
    uint8_t used;
    Game *node;
} InternEntry;

void game_intern_cache_init(size_t intern_size);
void game_intern_cache_free(void);

// Seradi left/right podle pointeru, aby klic byl stabilni
void game_intern_cache_prepare(Game *G);

// Vrati existujici identicky kanonicky uzel nebo vlozi a vrati G
Game* game_intern_cache_get(Game *G);

Game* game_intern_cache_prep_and_get(Game *G);

#endif // GAME_INTERN_CACHE_H
