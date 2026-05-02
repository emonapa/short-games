#ifndef SURREALS_H
#define SURREALS_H

#include <stdint.h>
#include "raw_game.h"
#include "dyadic.h"

// Inicializuje hash, může si přednaplnit známé pozice
void solver_initialize(const BaseGraph *g);

// Přesná hodnota pozice dané maskou hran (ignoruje player_to_move).
Dyadic solve(const BaseGraph *g, uint64_t live_mask);

#endif // SURREALS_H
