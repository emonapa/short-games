#ifndef HASH_GAME_H
#define HASH_GAME_H

#include <stdint.h>

#include "raw_game.h"
#include "dyadics.h"

void hash_game_init(void);
int  hash_game_lookup(edge_mask_t key, Dyadic *out_value);
void hash_game_insert(edge_mask_t key, Dyadic value);

#endif // HASH_GAME_H
