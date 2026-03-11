/* TODO
Z nejakeho duvodu se solveru FAKT nelibi grafy typu 0->1, 1->0, 0->1, ... / 0->1, 0->1, 0->1, ...
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "solver.h"
#include "position_cache.h"
#include "cgt.h"
#include "stack.h"
#include "game.h"
#include "singletons.h"

#include "config.h"
#include "memory.h"

// pointer na graf, se kterym solver pracuje
static const BaseGraph *g_global = NULL;

//void solver_initialize(const BaseGraph *g) {
void solver_initialize() {
    //g_global = g;

    size_t free_ram_bytes = get_size_free_memory();
    printf("free memory: %ldMB\n", free_ram_bytes/(1024*1024));
    //exit(0);
    size_t ram_to_use = free_ram_bytes * HOW_MUCH_MEM;


    /* init CGT memo tabulek (add/geq/canon/intern) */
    cgt_memo_init(free_ram_bytes);

    /* memo pro pozice */
    double pct_pos = 0.3;
    size_t pos_size = get_nearest_power_of_2((size_t)(ram_to_use * pct_pos) / sizeof(HashEntry));
    printf("Sizeof HashEntry = %ld\n", sizeof(HashEntry));
    printf("pos_size = %ldMB\n", pos_size/(1024*1024));

    position_cache_init(pos_size);
}

void solver_free() {
    position_cache_free();
    cgt_memo_free();
}

// Stack frame pro iterativni vypocet hodnoty
typedef struct {
    edge_mask_t live_mask;
    int stage;            // 0 = novy (init), 1 = prochazim tahy, 2 = vracim vysledek
    int current_edge;     // index hrany, u ktere jsme skoncili

    Game *left_opts[MAX_EDGES];
    int l_count;
    Game *right_opts[MAX_EDGES];
    int r_count;

    // Barva hrany, pro kterou se aktualne vynoril potomek (abychom vedeli, kam ho priradit)
    EdgeColor pending_child_color;

    Game *result;         // hotova hodnota pro tuto pozici
} SolverFrame;


// Iterativni vypocet hodnoty pozice
Game* solver_exact_solve(const BaseGraph *g, edge_mask_t live_mask) {
    g_global = g;

    TStack stack;
    stack_init(&stack, sizeof(SolverFrame));

    // koren jako hodnota
    SolverFrame root = (SolverFrame){0};
    root.live_mask = live_mask;
    root.stage = 0;
    root.result = NULL;
    Push(&stack, &root);

    while (!IsEmpty(&stack)) {
        SolverFrame *f = (SolverFrame *)Top(&stack);

        // STAGE 0: init, memo, base cases
        if (f->stage == 0) {
            Game *memo_val = NULL;

            if (position_cache_get(g, f->live_mask, &memo_val)) {
                f->result = memo_val;
                f->stage = 2;
                continue;
            }

            f->stage = 1;
            f->current_edge = 0;
            f->l_count = 0;
            f->r_count = 0;
            continue;
        }

        // STAGE 1: prochazeni tahu
        if (f->stage == 1) {
            int spawned_child = 0;

            for (int e = f->current_edge; e < g->num_edges; ++e) {
                if (!(f->live_mask & BIT(e))) continue;

                EdgeColor c = g->edges[e].color;

                edge_mask_t child_mask = f->live_mask & ~BIT(e);
                child_mask = cleanup_position(g, child_mask);

                // priprav rodice
                f->current_edge = e + 1;
                f->pending_child_color = c;

                // dite jako hodnota (zadny malloc)
                SolverFrame child = (SolverFrame){0};
                child.live_mask = child_mask;
                child.stage = 0;
                child.result = NULL;

                Push(&stack, &child);

                spawned_child = 1;
                break;
            }

            if (!spawned_child) {
                Game *G = game_make(f->left_opts, f->l_count, f->right_opts, f->r_count);

                G = game_canonicalize(G);
                position_cache_insert(g, f->live_mask, G);
                f->result = G;
                f->stage = 2;
            }
            continue;
        }

        // STAGE 2: hotovo, vrat vysledek rodici
        if (f->stage == 2) {
            Game *res = f->result;

            // odstran hotovy frame
            Pop(&stack);

            if (IsEmpty(&stack)) {
                stack_dtor(&stack);
                return res;
            }

            SolverFrame *parent = (SolverFrame *)Top(&stack);
            EdgeColor c = parent->pending_child_color;

            if (c == EDGE_BLUE || c == EDGE_GREEN) {
                parent->left_opts[parent->l_count++] = res;
            }
            if (c == EDGE_RED || c == EDGE_GREEN) {
                parent->right_opts[parent->r_count++] = res;
            }

            continue;
        }
    }
    stack_dtor(&stack);
    return game_zero();
}

// Pomocna funkce: Vrati pocet nalezenych nezavislych komponent a vyplni pole sub_masks
static int get_independent_components(const BaseGraph *g, edge_mask_t live_mask, edge_mask_t *sub_masks) {
    uint8_t visited[MAX_VERTICES] = {0};
    int comp_count = 0;

    // 1. Zvlastni pripad: Hrany, ktere jdou ze zeme zpet do zeme (smycky na vrcholu 0)
    // Kazda takova hrana je sama o sobe nezavisla hra.
    for (int e = 0; e < g->num_edges; e++) {
        if ((live_mask & BIT(e)) && g->edges[e].u == 0 && g->edges[e].v == 0) {
            sub_masks[comp_count++] = BIT(e);
            live_mask &= ~BIT(e); // Smazeme ji z masky
        }
    }

    // 2. Hledani komponent souvislosti pro vrcholy 1 az V-1 (Zemi ignorujeme)
    for (int i = 1; i < g->num_vertices; i++) {
        if (visited[i]) continue;

        uint8_t queue[MAX_VERTICES];
        int head = 0, tail = 0;
        queue[tail++] = i;
        visited[i] = 1;

        edge_mask_t comp_mask = 0;
        int found_edges = 0;

        // BFS pro ziskani vsech vrcholu v teto komponente
        while (head < tail) {
            int curr = queue[head++];

            // Najdi vsechny hrany dotykajici se 'curr'
            for (int e = 0; e < g->num_edges; e++) {
                if (!(live_mask & BIT(e))) continue;

                int u = g->edges[e].u;
                int v = g->edges[e].v;

                if (u == curr || v == curr) {
                    comp_mask |= BIT(e);
                    found_edges = 1;

                    // Souseda pridame do fronty (krome zeme 0)
                    int neighbor = (u == curr) ? v : u;
                    if (neighbor != 0 && !visited[neighbor]) {
                        visited[neighbor] = 1;
                        queue[tail++] = neighbor;
                    }
                }
            }
        }

        if (found_edges) {
            sub_masks[comp_count++] = comp_mask;
            live_mask &= ~comp_mask; // Vyradime zpracovane hrany
        }
    }

    // 3. Zbytek (napriklad zcela odpojene hrany, ktere nestihl smazat cleanup)
    if (live_mask > 0) {
        sub_masks[comp_count++] = live_mask;
    }

    return comp_count;
}

static void print_stats() {
    printf("canon_count = %ld\n", canon_items_count);
    printf("intern_count = %ld\n", intern_items_count);
    printf("add_count = %ld\n", add_items_count);
    printf("geq_count = %ld\n", geq_items_count);
    printf("pos_items_count = %ld\n", pos_items_count);
    printf("stack_count = %ld\n", stack_items_count);
    printf("[] make_count = %d\n", make_count);
}

Game* solver_solve_with_components(const BaseGraph *g, edge_mask_t live_mask) {
    if (live_mask == 0) return game_zero();

    edge_mask_t sub_masks[MAX_EDGES];
    int count = get_independent_components(g, live_mask, sub_masks);
    // Pokud se graf neda rozdelit, pustime ho proste klasicky solverem
    if (count <= 1) {
        Game *result = solver_exact_solve(g, live_mask);
        //printf("========================End stats========================\n");
        //print_stats();
        //printf("=========================================================\n");
        game_canonicalize(result);
        game_print_raw(result);
        return result;
    }

    //printf("[OPTIMALIZACE] Graf rozdelen na %d nezavislych komponent!\n", count);

    // Spocitame prvni komponentu
    static int counter = 0;
    //printf("Jedem po %d\n", counter++);
    //printf("========================End stats========================\n");
    //printf("-------[0] Průchod-------\n");
    Game *total_sum = solver_exact_solve(g, sub_masks[0]);
    //print_stats();

    // Spocitame dalsi a pricteme k mezivysledku
    for (int i = 1; i < count; i++) {
        Game *sub_game = solver_exact_solve(g, sub_masks[i]);
        total_sum = game_add(total_sum, sub_game);
        //printf("-------[%d] Průchod-------\n", i);
        //print_stats();
    }
    //printf("=========================================================\n");

    //printf("add_count = %d\n", add_count++);
    game_canonicalize(total_sum);
    //game_print_raw(total_sum);
    return total_sum;
}
