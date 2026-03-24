#ifndef SOLVER_H
#define SOLVER_H

#include "raw_game.h"
#include "short_game.h"

void solver_initialize(float how_much_mem);
void solver_free();

Game* solve_component(const BaseGraph *g, edge_mask_t live_mask);

// Rozdeli pozici na nezavisle komponenty a secte je
Game* solve(const BaseGraph *g, edge_mask_t live_mask);

#endif // SOLVER_H
