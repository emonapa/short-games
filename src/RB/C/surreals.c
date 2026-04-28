#include <stdint.h>
#include <stdio.h>

#include "surreals.h"
#include "hash_game.h"

// tady drzime pointer na graf, se kterým solver pracuje
static const BaseGraph *g_global = NULL;

// Stack frame pro iterativní výpočet hodnoty
typedef struct {
    uint64_t live_mask;
    int      stage;            // 0 = nový, 1 = procházím tahy, 2 = hotovo
    int      edge_index;       // kde jsme v procházení hran
    EdgeColor last_move_color; // barva hrany, kterou jsme šli do potomka

    int      has_left;
    int      has_right;
    Dyadic   l_max;            // max hodnota z levých možností (modré tahy)
    Dyadic   r_min;            // min hodnota z pravých možností (červené tahy)

    Dyadic   result;           // hotová hodnota pro tuto pozici
} SolverFrame;

void solver_initialize(const BaseGraph *g) {
    g_global = g;
    hash_game_init();
}


// Iterativní výpočet hodnoty pozice (bez rekurze)
Dyadic solve(const BaseGraph *g, uint64_t live_mask) {
    SolverFrame stack[MAX_EDGES + 1];
    int top = 0;

    stack[top++] = (SolverFrame){ .live_mask = live_mask, .stage = 0 };

    while (top > 0) {
        SolverFrame *f = &stack[top - 1];

        // -- Stage 0: Memo / triviální případ --
        if (f->stage == 0) {
            PositionKey key = { f->live_mask };
            Dyadic v;
            if (hash_game_lookup(&key, &v)) { f->result = v; f->stage = 2; }
            else if (!f->live_mask)         { f->result = dyadic_make(0, 0); f->stage = 2; }
            else                             f->stage = 1;
        }

        // -- Stage 1: Procházení hran --
        if (f->stage == 1) {
            int pushed = 0;
            for (int e = f->edge_index; e < g->num_edges; e++) {
                if (!(f->live_mask & (1ULL << e))) continue;

                f->edge_index      = e + 1;
                f->last_move_color = g->edges[e].color;
                uint64_t child     = cleanup_position(g, f->live_mask & ~(1ULL << e));

                stack[top++] = (SolverFrame){ .live_mask = child, .stage = 0 };
                pushed = 1;
                break;
            }

            if (!pushed) {
                Dyadic r;
                if      (!f->has_left && !f->has_right)       r = dyadic_make(0, 0);
                else if ( f->has_left && !f->has_right)       r = dyadic_simplest_above(f->l_max);
                else if (!f->has_left &&  f->has_right)       r = dyadic_simplest_below(f->r_min);
                else if (dyadic_cmp(f->l_max, f->r_min) < 0) r = dyadic_simplest_between(f->l_max, f->r_min);
                else                                          r = f->l_max;
                f->result = r;
                f->stage  = 2;
            }
        }

        // -- Stage 2: Propagace výsledku rodiči --
        if (f->stage == 2) {
            Dyadic value = f->result;
            hash_game_insert(&(PositionKey){ f->live_mask }, value);
            top--;
            if (top == 0) return value;

            SolverFrame *p = &stack[top - 1];
            if (p->last_move_color == EDGE_BLUE) {
                if (!p->has_left  || dyadic_cmp(value, p->l_max) > 0) { p->l_max = value; p->has_left  = 1; }
            } else {
                if (!p->has_right || dyadic_cmp(value, p->r_min) < 0) { p->r_min = value; p->has_right = 1; }
            }
        }
    }

    return dyadic_make(0, 0);
}

/*
Dyadic solve(const BaseGraph *g, uint64_t live_mask) {
    PositionKey key = { live_mask };
    Dyadic memo;
    if (hash_game_lookup(&key, &memo)) return memo;
    if (!live_mask) return dyadic_make(0, 0);

    int has_left = 0, has_right = 0;
    Dyadic l_max, r_min;

    for (int e = 0; e < g->num_edges; e++) {
        if (!(live_mask & (1ULL << e))) continue;

        uint64_t child = cleanup_position(g, live_mask & ~(1ULL << e));
        Dyadic val = solve(g, child);

        if (g->edges[e].color == EDGE_BLUE) {
            if (!has_left  || dyadic_cmp(val, l_max) > 0) { l_max = val; has_left  = 1; }
        } else {
            if (!has_right || dyadic_cmp(val, r_min) < 0) { r_min = val; has_right = 1; }
        }
    }

    Dyadic result;
    if      (!has_left && !has_right)              result = dyadic_make(0, 0);
    else if ( has_left && !has_right)              result = dyadic_simplest_above(l_max);
    else if (!has_left &&  has_right)              result = dyadic_simplest_below(r_min);
    else if (dyadic_cmp(l_max, r_min) < 0)        result = dyadic_simplest_between(l_max, r_min);
    else                                           result = l_max;

    hash_game_insert(&key, result);
    return result;
}
*/
