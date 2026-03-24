#ifndef GAME_OPERATIONS_CACHE_H
#define GAME_OPERATIONS_CACHE_H

#include <stdint.h>
#include "short_game.h"

typedef struct {
    uint8_t used;
    uintptr_t a;
    uintptr_t b;
    Game *value;
} AddEntry;

typedef struct {
    uint8_t used;
    uint8_t value;
    uintptr_t a;
    uintptr_t b;
} GeqEntry;

// init/clear
void game_operations_cache_init(size_t geq_size, size_t add_size);
void game_operations_cache_free_all(void);

// add memo: (A,B) -> Game*
int  game_add_cache_get(Game *A, Game *B, Game **out);
void game_add_cache_put(Game *A, Game *B, Game *value);
void game_add_cache_clear(void);

// geq memo: (A,B) -> 0/1
int  game_geq_cache_get(Game *A, Game *B, uint8_t *out);
void game_geq_cache_put(Game *A, Game *B, uint8_t value);
void game_geq_cache_clear(void);

#endif // GAME_OPERATIONS_CACHE_H
