#ifndef DYADIC_H
#define DYADIC_H

#include <stdint.h>
#include "dyadics.h"
#include "raw_game.h"

// Základní operace
edge_mask_t floor_div_pow2_i128(edge_mask_t x, int d);
edge_mask_t ceil_div_pow2_i128(edge_mask_t x, int d);
edge_mask_t floor_scaled(Dyadic a, int k);
edge_mask_t ceil_scaled(Dyadic a, int k);

#endif // DYADIC_H
