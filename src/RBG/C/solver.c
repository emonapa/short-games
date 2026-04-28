#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "config.h"
#include "error.h"

#include "solver.h"
#include "position_cache.h"
#include "short_game.h"
#include "stack.h"
#include "raw_game.h"
#include "singletons.h"

// pointer na graf se kterym solver pracuje
static const BaseGraph *g_global = NULL;
static float g_memory_multiplier = 0;

void solver_initialize(float memory_multiplier) {
    if (memory_multiplier > 1 || memory_multiplier <= 0) error_exit(ERR_OTHER, "%f is invalid fraction argument.\n", memory_multiplier);
    g_memory_multiplier = memory_multiplier;

    size_t free_ram_bytes = get_size_free_memory();
    size_t ram_to_use = free_ram_bytes * memory_multiplier;

    short_game_init(free_ram_bytes, memory_multiplier);

    // vypocet prvku ktere se vlezou do RAM * PCT_POS
    size_t pos_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_POS) / sizeof(HashEntry));

    position_cache_init(pos_size);
}

void solver_free() {
    position_cache_free();
    short_game_free();
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

/*
Game* solve_component(const BaseGraph *g, edge_mask_t live_mask) {
    if (g == NULL) error_exit(ERR_NULL_POINTER, "");

    // Memoizace
    Game *memo = NULL;
    if (position_cache_get(g, live_mask, &memo))
        return memo;

    Game *left_opts[MAX_EDGES];
    Game *right_opts[MAX_EDGES];
    int l_count = 0, r_count = 0;

    // Rekurzivní průchod všemi tahy
    for (int e = 0; e < g->num_edges; ++e) {
        if (!(live_mask & BIT(e))) continue;

        EdgeColor c = g->edges[e].color;
        edge_mask_t child_mask = cleanup_position(g, live_mask & ~BIT(e));

        Game *child = solve_component(g, child_mask);

        if (c == EDGE_BLUE  || c == EDGE_GREEN) left_opts[l_count++]  = child;
        if (c == EDGE_RED   || c == EDGE_GREEN) right_opts[r_count++] = child;
    }

    Game *G = game_canonicalize(game_make(left_opts, l_count, right_opts, r_count));
    position_cache_insert(g, live_mask, G);
    return G;
}
*/
// Iterativni vypocet hodnoty pozice

Game* solve_component(const BaseGraph *g, edge_mask_t live_mask) {
    if (g == NULL) error_exit(ERR_NULL_POINTER, "");

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

                // dite jako hodnota
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
                uint64_t lower = (f->live_mask << 64) >> 64;
                uint64_t higher = f->live_mask >> 64;
                //printf("C maska pri insertu do cache: %zu%zu\n", higher, lower);
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

    // 1. Special case: Hrany, ktere jdou ze zeme zpet do zeme (smycky na vrcholu 0)
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
        warning("There exists edge with no path to the ground.\n");
        sub_masks[comp_count++] = live_mask;
    }

    return comp_count;
}

static void print_stats() {
    printf("[CACHE] canon_count     = %ld\n", canon_items_count);
    printf("[CACHE] intern_count    = %ld\n", intern_items_count);
    printf("[CACHE] add_count       = %ld\n", add_items_count);
    printf("[CACHE] geq_count       = %ld\n", geq_items_count);
    printf("[CACHE] pos_items_count = %ld\n", pos_items_count);
    printf("[META] stack_count      = %ld\n", stack_items_count);
    printf("[META] make_count       = %d\n", make_count);
}

//#define PRINT_RESULT

Game* solve(const BaseGraph *g, edge_mask_t live_mask) {
    g_global = g;
    if (live_mask == 0) return game_zero();

    edge_mask_t sub_masks[MAX_EDGES];
    int count = get_independent_components(g, live_mask, sub_masks);

    Game *total_sum = game_zero();


#ifdef PRINT_RESULT
    printf("======================END RESULT==========================\n");
#endif
    // Spocitame dalsi a pricteme k mezivysledku
    for (int i = 0; i < count; i++) {
        Game *sub_game = solve_component(g, sub_masks[i]);
        total_sum = game_add(total_sum, sub_game);
#ifdef PRINT_RESULT
       printf("-------------[%d] Průchod-------------\n", i);
       print_stats();
#endif
        // Grafy se budou pravdepodobne lisit, takze cache resetneme
        // Ale hinty a edu mode budou EXTREMNE pomale, achjo...
        //solver_free();
        //solver_initialize(g_memory_multiplier);
    }
#ifdef PRINT_RESULT
    printf("=========================================================\n");
#endif

    game_canonicalize(total_sum);

#ifdef PRINT_RESULT
    const char *game_string = game_get_string(total_sum, FORMAT_FORMATED);
    printf("Result: %s", game_string);
#endif

    return total_sum;
}
