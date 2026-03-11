#include <stdio.h>
#include "game.h"
#include "solver.h"
#include "cgt.h"

int main(void) {
    BaseGraph g;
    g.num_vertices = 3;
    g.num_edges = 2;

    // Testovací graf: Zelená hrana ze země, na ní červená hrana.
    // Tohle je typická fuzzy pozice.
    g.edges[0] = (Edge){0, 1, EDGE_RED};
    g.edges[1] = (Edge){1, 2, EDGE_GREEN};

    Position start;
    start.live_mask = BIT(0) | BIT(1);
    start.player_to_move = 0;

    solver_initialize(&g);
    //printf("Zacinam resit pozici s maskou %lx...\n", start.live_mask);

    Game *result = solver_exact_solve(&g, start.live_mask);

    // Vypíše rovnou, do které ze 4 tříd hra spadá
    game_print_outcome(result);

    return 0;
}
