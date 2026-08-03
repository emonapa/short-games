/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef GAME_CANON_CACHE_H
#define GAME_CANON_CACHE_H

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

#endif // GAME_CANON_CACHE_H
