#ifndef SOLVER_H
#define SOLVER_H
#include "game.h"
#include "cgt.h"

//void solver_initialize(const BaseGraph *g);
void solver_initialize();
void solver_free();

Game* solver_exact_solve(const BaseGraph *g, edge_mask_t live_mask);
// Hlavni wrapper: Rozdeli pozici na nezavisle komponenty a secte je
Game* solver_solve_with_components(const BaseGraph *g, edge_mask_t live_mask);

#endif // SOLVER_H
