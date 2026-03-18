#ifndef SOLVER_H
#define SOLVER_H
#include "game.h"
#include "cgt.h"

void solver_initialize(float how_much_mem);
void solver_free();

Game* solve_component(const BaseGraph *g, edge_mask_t live_mask);
// Hlavni wrapper: Rozdeli pozici na nezavisle komponenty a secte je
Game* solve(const BaseGraph *g, edge_mask_t live_mask);

#endif // SOLVER_H
