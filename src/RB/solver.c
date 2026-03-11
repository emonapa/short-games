#include <stdint.h>
#include <stdio.h>

#include "solver.h"
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
    hash_game_preload_simple_positions(g);
}

// Iterativní výpočet hodnoty pozice (bez rekurze)
Dyadic solver_exact_solve(const BaseGraph *g, uint64_t live_mask) {
    g_global = g; // pro jistotu, kdyby solver_initialize nebyl zavolán

    SolverFrame stack[MAX_EDGES + 1];
    int top = 0;

    stack[top].live_mask = live_mask;
    stack[top].stage = 0;
    stack[top].edge_index = 0;
    stack[top].has_left = 0;
    stack[top].has_right = 0;
    top++;

    while (top > 0) {
        SolverFrame *f = &stack[top - 1];

        if (f->stage == 2) {
            // máme výsledek pro tuto pozici, uložíme do hash a propagujeme nahoru
            PositionKey key;
            key.live_mask = f->live_mask;
            hash_game_insert(&key, f->result);
            Dyadic value = f->result;

            top--;

            if (top == 0) {
                return value; // kořen
            }

            SolverFrame *parent = &stack[top - 1];

            // aktualizuj agregátora rodiče podle barvy tahu
            if (parent->last_move_color == EDGE_BLUE) {
                if (!parent->has_left || dyadic_cmp(value, parent->l_max) > 0) {
                    parent->l_max = value;
                    parent->has_left = 1;
                }
            } else {
                if (!parent->has_right || dyadic_cmp(value, parent->r_min) < 0) {
                    parent->r_min = value;
                    parent->has_right = 1;
                }
            }
            continue;
        }

        if (f->stage == 0) {
            // nový frame, zkusíme memoizaci
            PositionKey key;
            key.live_mask = f->live_mask;

            Dyadic memo_val;
            if (hash_game_lookup(&key, &memo_val)) {
                f->result = memo_val;
                f->stage = 2;
                continue;
            }

            // pokud je pozice prázdná, hodnota je 0
            if (f->live_mask == 0) {
                f->result = dyadic_make(0, 0);
                f->stage = 2;
                continue;
            }

            // inicializace agregátorů
            f->has_left = 0;
            f->has_right = 0;
            f->edge_index = 0;
            f->stage = 1;
            continue;
        }

        // stage == 1 -> procházíme všechny možné tahy (levé i pravé)
        int e;
        for (e = f->edge_index; e < g_global->num_edges; ++e) {
            if (!(f->live_mask & (1ULL << e))) continue; // hrana nežije

            EdgeColor c = g_global->edges[e].color;

            uint64_t child_mask = f->live_mask & ~(1ULL << e);
            child_mask = cleanup_position(g_global, child_mask);

            // připrav rodiče, aby po návratu z potomka věděl, co agreguje
            f->edge_index = e + 1;
            f->last_move_color = c;

            // založ frame pro potomka
            if (top >= MAX_EDGES) {
                //pojistka
                return dyadic_make(0, 0);
            }

            stack[top].live_mask = child_mask;
            stack[top].stage = 0;
            stack[top].edge_index = 0;
            stack[top].has_left = 0;
            stack[top].has_right = 0;
            top++;

            goto next_iteration;
        }

        // žádné další hrany k prozkoumání, spočítat hodnotu z agregátorů
        if (!f->has_left && !f->has_right) {
            f->result = dyadic_make(0, 0);
        } else if (f->has_left && !f->has_right) {
            f->result = dyadic_simplest_above(f->l_max);
        } else if (!f->has_left && f->has_right) {
            f->result = dyadic_simplest_below(f->r_min);
        } else {
            if (dyadic_cmp(f->l_max, f->r_min) < 0) {
                f->result = dyadic_simplest_between(f->l_max, f->r_min);
            } else {
                f->result = f->l_max; // degenerace :P
            }
        }
        f->stage = 2;

    next_iteration:
        continue;
    }

    return dyadic_make(0, 0);
}
