#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "game.h"
#include "solver.h"

int main(void) {
    BaseGraph g;

    g.num_vertices = 8;
    g.num_edges = 7;

    g.edges[0] = (Edge){0, 1, EDGE_RED};
    g.edges[1] = (Edge){1, 2, EDGE_RED};
    g.edges[2] = (Edge){2, 3, EDGE_RED};

    g.edges[3] = (Edge){0, 4, EDGE_BLUE};
    g.edges[4] = (Edge){4, 5, EDGE_RED};

    g.edges[5] = (Edge){0, 6, EDGE_RED};
    g.edges[6] = (Edge){6, 7, EDGE_BLUE};


/*
    g.num_vertices = 12;
    g.num_edges = 11;

    g.edges[0] = (Edge){0, 1, EDGE_BLUE};
    g.edges[1] = (Edge){1, 2, EDGE_BLUE};
    g.edges[2] = (Edge){2, 3, EDGE_BLUE};
    g.edges[3] = (Edge){2, 4, EDGE_BLUE};

    g.edges[4] = (Edge){3, 5, EDGE_BLUE};
    g.edges[5] = (Edge){3, 6, EDGE_BLUE};

    g.edges[6] = (Edge){1, 7, EDGE_BLUE};
    g.edges[7] = (Edge){7, 8, EDGE_BLUE};
    g.edges[8] = (Edge){7, 9, EDGE_BLUE};

    g.edges[9] = (Edge){0, 10, EDGE_BLUE};
    g.edges[10] = (Edge){10, 11, EDGE_RED};
*/

    //g.edges[8] = (Edge){6, 7, EDGE_BLUE};


/*
    g.num_vertices = 13;
    g.num_edges = 13;

    g.edges[0] = (Edge){0, 1, EDGE_BLUE};
    g.edges[1] = (Edge){1, 2, EDGE_RED};
    g.edges[2] = (Edge){2, 3, EDGE_BLUE};

    g.edges[3] = (Edge){0, 4, EDGE_BLUE};
    g.edges[4] = (Edge){4, 5, EDGE_RED};
    g.edges[5] = (Edge){5, 6, EDGE_BLUE};
    g.edges[6] = (Edge){6, 7, EDGE_BLUE};

    g.edges[7] = (Edge){0, 8, EDGE_BLUE};
    g.edges[8] = (Edge){8, 9, EDGE_RED};
    g.edges[9] = (Edge){9, 10, EDGE_BLUE};
    g.edges[10] = (Edge){10, 11, EDGE_BLUE};
    g.edges[11] = (Edge){11, 12, EDGE_RED};

    g.edges[12] = (Edge){1, 4, EDGE_RED};
*/

    //g.edges[2] = (Edge){1, 2, EDGE_BLUE};
    //g.edges[3] = (Edge){1, 3, EDGE_RED};
    //g.edges[4] = (Edge){3, 4, EDGE_RED};


    Position start;
    //start.live_mask = (1ULL << (g.num_edges + 1)) - 1;
    start.live_mask = 0;
    for (int i = 0; i < g.num_edges; i++) {
        start.live_mask |= (1ULL << i);
    }
    printf("mask: %lx\n", start.live_mask);
    //start.live_mask = (1ULL << 0) | (1ULL << 1) | (1ULL << 2) | (1ULL << 3); // 0b111
    start.player_to_move = 0; // modrý

    // printf("Startovni pozice:\n");
    // print_position(&g, &start);

    //printf("Hodnota startovni pozice: %d / 2^%d\n",
    //       val.num, val.exp);


    // SOLVER
    solver_initialize(&g);
    Dyadic val = solver_exact_solve_position(&g, &start);

    double result = val.num / pow(2, val.exp);
    printf("Result: %f\n", result);
    if (val.num > 0) {
        printf("Vyhoda modreho (Left)\n");
    } else if (val.num < 0) {
        printf("Vyhoda cerveneho (Right)\n");
    } else {
        printf("Neutralni pozice (0)\n");
    }



    export_graph_svg(&g, &start, "graph_before.svg");

    // Modrý smaže hranu 0 (0-1)
    //Position p1 = do_move(&g, &start, 2);

    //printf("Po tahu: smazana hrana 0 (0-1)\n");
    //print_position(&g, &p1);
    //export_graph_svg(&g, &p1, "graph_after.svg");

    return 0;
}
