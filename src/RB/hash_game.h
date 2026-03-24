#ifndef HASH_GAME_H
#define HASH_GAME_H

#include <stdint.h>
#include "game.h"
#include "solver.h"
#include "dyadic.h"

typedef struct {
    uint64_t live_mask;
} PositionKey;

void hash_game_init(void);
int  hash_game_lookup(const PositionKey *key, Dyadic *out_value);
void hash_game_insert(const PositionKey *key, Dyadic value);

#endif // HASH_GAME_H
