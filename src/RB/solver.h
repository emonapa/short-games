#ifndef SOLVER_H
#define SOLVER_H

#include <stdint.h>
#include "game.h"
#include "dyadic.h"

// Inicializuje hash, může si přednaplnit známé pozice
void solver_initialize(const BaseGraph *g);

// Přesná hodnota pozice dané maskou hran (ignoruje player_to_move).
Dyadic solver_exact_solve(const BaseGraph *g, uint64_t live_mask);

// wrapper přes Position (bere jen live_mask)
static inline Dyadic solver_exact_solve_position(const BaseGraph *g,
                                                 const Position *p) {
    return solver_exact_solve(g, p->live_mask);
}

#endif // SOLVER_H
