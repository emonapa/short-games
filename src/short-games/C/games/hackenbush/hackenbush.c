/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hackenbushe s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hackenbush Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include <stdio.h>

#include "error.h"
#include "darray.h"

#include "hackenbush.h"
#include "raw_game.h"

// Pomocná funkce pro adjacency list
void build_adjacency(const BaseGraph *g, edge_mask_t live_mask, uint8_t *deg, uint8_t adj[][MAX_EDGES]) {

    if (g == NULL || deg == NULL || adj == NULL) error_exit(ERR_NULL_POINTER, "");

    for (int i = 0; i < g->num_vertices; ++i) {
        deg[i] = 0;
    }

    for (int e = 0; e < g->num_edges; ++e) {
        if (!(live_mask & BIT(e))) continue;
        uint8_t u = g->edges[e].u;
        uint8_t v = g->edges[e].v;
        adj[u][deg[u]++] = v;
        adj[v][deg[v]++] = u;
    }
}

// Vyhodí všechny komponenty nepropojené se zemí (vrchol 0)
edge_mask_t cleanup_position(const BaseGraph *g, edge_mask_t live_mask) {
    if (g == NULL) error_exit(ERR_NULL_POINTER, "");

    uint8_t reachable[MAX_VERTICES] = {0};
    uint8_t queue[MAX_VERTICES];
    uint8_t qh = 0, qt = 0;

    uint8_t deg[MAX_VERTICES];
    uint8_t adj[MAX_VERTICES][MAX_EDGES];

    build_adjacency(g, live_mask, deg, adj);

    reachable[0] = 1;
    queue[qt++] = 0;

    while (qh < qt) {
        uint8_t v = queue[qh++];
        for (uint8_t i = 0; i < deg[v]; ++i) {
            uint8_t u = adj[v][i];
            if (!reachable[u]) {
                reachable[u] = 1;
                queue[qt++] = u;
            }
        }
    }

    uint64_t new_mask = 0;
    for (int e = 0; e < g->num_edges; ++e) {
        if (!(live_mask & BIT(e))) continue;
        uint8_t u = g->edges[e].u;
        uint8_t v = g->edges[e].v;
        if (reachable[u] && reachable[v]) {
            new_mask |= BIT(e);
        }
    }
    return new_mask;
}

/*
 * ------------------------------------------------------------------------
 * Tady začíná implementace na řešič krátkých her.
 * Tyto funkce musí být implementové pro správné řešení hry.
 * Výchází ze souboru "raw_game.h".
 * ------------------------------------------------------------------------
 */
int num_moves(RawGame_t raw_game) {
    if (raw_game == NULL) error_exit(ERR_NULL_POINTER, "");
    BaseGraph *g = (BaseGraph*)(raw_game);
    return g->num_edges;
}

Position_t do_move_left(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");

    BaseGraph *g = (BaseGraph *)raw_game;
    Position *pos = (Position *)position;

    Position *pos_after = malloc(sizeof *pos_after);
    if (pos_after == NULL) return NULL;

    pos_after->live_mask = cleanup_position(g, pos->live_mask & ~BIT(e));

    return pos_after;
}

Position_t do_move_right(RawGame_t raw_game, Position_t position, int e) {
    return do_move_left(raw_game, position, e);
}

int can_left_move(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");
    BaseGraph *g = (BaseGraph*)(raw_game);
    Position *pos = (Position *)(position);

    if (!IS_BIT_ACTIVE(pos->live_mask, e)) return 0;
    EdgeColor c = g->edges[e].color;
    return (c == EDGE_BLUE || c == EDGE_GREEN);
}

int can_right_move(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");
    BaseGraph *g = (BaseGraph*)(raw_game);
    Position *pos = (Position *)(position);

    if (!IS_BIT_ACTIVE(pos->live_mask, e)) return 0;
    EdgeColor c = g->edges[e].color;
    return (c == EDGE_RED || c == EDGE_GREEN);
}

uint64_t hash_raw_game_position(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");
    BaseGraph *g = (BaseGraph *)raw_game;
    Position *pos = (Position *)position;
    uint64_t h = 14695981039346656037ULL;
    h ^= g->edges[e].u;     h *= 1099511628211ULL;
    h ^= g->edges[e].v;     h *= 1099511628211ULL;
    h ^= g->edges[e].color; h *= 1099511628211ULL;
    return h;
}

static Position_t create_position(edge_mask_t mask) {
    Position *component_mask = malloc(sizeof *component_mask);
    if (component_mask == NULL) return NULL;
    component_mask->live_mask = mask;
    return component_mask;
}

int get_independent_components(RawGame_t raw_game, Position_t position, Position_t *sub_masks[]) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");
    BaseGraph *g = (BaseGraph *)raw_game;
    Position *pos = (Position *)position;
    edge_mask_t live_mask = pos->live_mask;

    Position_t *da_sub_masks = NULL;
    uint8_t visited[MAX_VERTICES] = {0};
    int comp_count = 0;
    // 1. Special case: Hrany, ktere jdou ze zeme zpet do zeme (smycky na vrcholu 0)
    // Kazda takova hrana je sama o sobe nezavisla hra.
    for (int e = 0; e < g->num_edges; e++) {
        if ((live_mask & BIT(e)) && g->edges[e].u == 0 && g->edges[e].v == 0) {
            Position_t new_mask = create_position(BIT(e));
            if (new_mask == NULL) {
                live_mask &= ~BIT(e);
                continue;
            }
            da_push(da_sub_masks, new_mask);
            comp_count++;
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
            Position_t new_mask = create_position(comp_mask);
            if (new_mask == NULL) {
                live_mask &= ~comp_mask;
                continue;
            }
            da_push(da_sub_masks, new_mask);
            comp_count++;
            live_mask &= ~comp_mask; // Vyradime zpracovane hrany
        }
    }

    // 3. Zbytek (napriklad zcela odpojene hrany, ktere nestihl smazat cleanup)
    if (live_mask > 0) {
        warning("There exists edge with no path to the ground.\n");
        Position_t new_mask = create_position(live_mask);
        if (new_mask == NULL) return comp_count;
        da_push(da_sub_masks, new_mask);
        comp_count++;
    }
    *sub_masks = da_sub_masks;
    return comp_count;
}
