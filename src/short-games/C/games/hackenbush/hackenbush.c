/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hackenbushe s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hackenbush Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "darray.h"

#include "hackenbush.h"
#include "raw_game.h"

/*
 * Builds an adjacency list for the currently active edges.
 *
 * Only edges present in live_mask are inserted into the adjacency list.
 * The graph is undirected, therefore every edge is inserted for both
 * of its endpoint vertices.
 */
void build_adjacency(const BaseGraph *g, edge_mask_t live_mask, uint8_t *deg, uint8_t adj[][MAX_EDGES]) {
    if (g == NULL || deg == NULL || adj == NULL) error_exit(ERR_NULL_POINTER, "");

    for (int i = 0; i < g->num_vertices; ++i) {
        deg[i] = 0;
    }

    for (int e = 0; e < g->num_edges; ++e) {
        if (!IS_BIT_ACTIVE(live_mask, e)) continue;

        uint8_t u = g->edges[e].u;
        uint8_t v = g->edges[e].v;

        adj[u][deg[u]++] = v;
        adj[v][deg[v]++] = u;
    }
}

/*
 * Removes all edges that are no longer connected to the ground vertex.
 *
 * In Hackenbush, the ground is represented by vertex 0. After a move,
 * some parts of the graph may become disconnected from the ground.
 * These parts are not part of the remaining position and must be removed.
 */
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

    edge_mask_t new_mask = 0;

    for (int e = 0; e < g->num_edges; ++e) {
        if (!IS_BIT_ACTIVE(live_mask, e)) continue;

        uint8_t u = g->edges[e].u;
        uint8_t v = g->edges[e].v;

        if (reachable[u] && reachable[v]) {
            new_mask = SET_BIT_AT(new_mask, e);
        }
    }

    return new_mask;
}

/*
 * ------------------------------------------------------------------------
 * Implementation of the short-games solver interface.
 *
 * These functions are required by the generic short-games solver and are
 * based on the interface declared in raw_game.h.
 * ------------------------------------------------------------------------
 */

int num_moves(RawGame_t raw_game) {
    if (raw_game == NULL) error_exit(ERR_NULL_POINTER, "");

    BaseGraph *g = (BaseGraph *)raw_game;
    return g->num_edges;
}

Position_t do_move_left(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");

    BaseGraph *g = (BaseGraph *)raw_game;
    Position *pos = (Position *)position;

    Position *pos_after = malloc(sizeof *pos_after);
    if (pos_after == NULL) return NULL;

    /*
     * A move removes the selected edge. The resulting position is then
     * cleaned up, because removing one edge can disconnect other edges
     * from the ground.
     */
    pos_after->live_mask = cleanup_position(g, RESET_BIT_AT(pos->live_mask, e));

    return pos_after;
}

Position_t do_move_right(RawGame_t raw_game, Position_t position, int e) {
    return do_move_left(raw_game, position, e);
}

int can_left_move(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");

    BaseGraph *g = (BaseGraph *)raw_game;
    Position *pos = (Position *)position;

    if (!IS_BIT_ACTIVE(pos->live_mask, e)) return 0;

    EdgeColor c = g->edges[e].color;
    return (c == EDGE_BLUE || c == EDGE_GREEN);
}

int can_right_move(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");

    BaseGraph *g = (BaseGraph *)raw_game;
    Position *pos = (Position *)position;

    if (!IS_BIT_ACTIVE(pos->live_mask, e)) return 0;

    EdgeColor c = g->edges[e].color;
    return (c == EDGE_RED || c == EDGE_GREEN);
}

uint64_t hash_raw_game_position(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");

    BaseGraph *g = (BaseGraph *)raw_game;

    uint64_t h = 14695981039346656037ULL;

    h ^= g->edges[e].u;
    h *= 1099511628211ULL;

    h ^= g->edges[e].v;
    h *= 1099511628211ULL;

    h ^= g->edges[e].color;
    h *= 1099511628211ULL;

    return h;
}

static Position_t create_position(edge_mask_t mask) {
    Position *component_mask = malloc(sizeof *component_mask);
    if (component_mask == NULL) return NULL;

    component_mask->live_mask = mask;
    return component_mask;
}

/*
 * Splits the current Hackenbush position into independent components.
 *
 * Independent components can be evaluated separately by the short-games
 * solver. The ground vertex itself is ignored while finding components,
 * because different branches connected only through the ground represent
 * independent subgames.
 */
int get_independent_components(RawGame_t raw_game, Position_t position, Position_t *sub_masks[]) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");

    BaseGraph *g = (BaseGraph *)raw_game;
    Position *pos = (Position *)position;
    edge_mask_t live_mask = pos->live_mask;

    Position_t *da_sub_masks = NULL;
    uint8_t visited[MAX_VERTICES] = {0};
    int comp_count = 0;

    /*
     * Special case:
     * Edges that start and end in the ground vertex are loops on vertex 0.
     * Each such loop forms an independent game by itself.
     */
    for (int e = 0; e < g->num_edges; e++) {
        if (IS_BIT_ACTIVE(live_mask, e) && g->edges[e].u == 0 && g->edges[e].v == 0) {
            Position_t new_mask = create_position(BIT(e));

            if (new_mask == NULL) {
                live_mask = RESET_BIT_AT(live_mask, e);
                continue;
            }

            da_push(da_sub_masks, new_mask);
            comp_count++;

            live_mask = RESET_BIT_AT(live_mask, e);
        }
    }

    /*
     * Find connected components for vertices 1 to V - 1.
     *
     * Vertex 0, the ground, is intentionally not marked as part of any
     * component. This prevents separate branches connected only through
     * the ground from being merged into one component.
     */
    for (int i = 1; i < g->num_vertices; i++) {
        if (visited[i]) continue;

        uint8_t queue[MAX_VERTICES];
        int head = 0;
        int tail = 0;

        queue[tail++] = i;
        visited[i] = 1;

        edge_mask_t comp_mask = 0;
        int found_edges = 0;

        /*
         * Breadth-first search over non-ground vertices.
         *
         * Whenever an active edge touches the current vertex, the edge is
         * added to the component mask. The opposite endpoint is inserted
         * into the queue unless it is the ground vertex.
         */
        while (head < tail) {
            int curr = queue[head++];

            for (int e = 0; e < g->num_edges; e++) {
                if (!IS_BIT_ACTIVE(live_mask, e)) continue;

                int u = g->edges[e].u;
                int v = g->edges[e].v;

                if (!(u == curr || v == curr)) continue;

                comp_mask = SET_BIT_AT(comp_mask, e);
                found_edges = 1;

                int neighbor = (u == curr) ? v : u;

                if (neighbor != 0 && !visited[neighbor]) {
                    visited[neighbor] = 1;
                    queue[tail++] = neighbor;
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

            /*
             * Remove the processed component from live_mask so that it is
             * not processed again later.
             */
            live_mask &= ~comp_mask;
        }
    }

    /*
     * Remaining edges should normally not exist after cleanup_position().
     * If they do, they are kept as one additional component and a warning
     * is emitted.
     */
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
