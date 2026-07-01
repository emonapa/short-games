#ifndef CONVERT_H
#define CONVERT_H

#include "convert_interface/raw_game.h"
#include "../core/short_game.h"

void convert_init(float memory_multiplier);
void convert_free(void);

// Solves a single connected component of the raw Hotpotch position.
Game* convert_component(RawGame_t raw_game, Position_t position);

// Splits the position into independent components and sums their game values.
Game* convert(RawGame_t raw_game, Position_t position);

#endif // CONVERT_H
