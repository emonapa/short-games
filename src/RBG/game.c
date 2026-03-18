#include <stdio.h>
#include <stdint.h>

#include "error.h"
#include "game.h"

// Pomocná funkce pro adjacency list
void build_adjacency(const BaseGraph *g, edge_mask_t live_mask,
                            uint8_t *deg, uint8_t adj[][MAX_EDGES]) {

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

// Provede tah: smaže hranu e, udělá cleanup a přepne hráče
Position do_move(const BaseGraph *g, const Position *p, int edge_index) {
    Position np = *p;
    np.live_mask &= ~BIT(edge_index);     // smažu hranu
    np.live_mask = cleanup_position(g, np.live_mask); // cleanup
    np.player_to_move ^= 1;                    // druhý hráč
    return np;
}


void print_position(const BaseGraph *g, const Position *p) {
    printf("Pozice: player_to_move = %u\n", p->player_to_move);
    printf("Zive hrany:\n");
    for (int e = 0; e < g->num_edges; ++e) {
        if (!(p->live_mask & BIT(e))) continue;
        Edge edge = g->edges[e];
        const char *col = (edge.color == EDGE_BLUE) ? "blue" : "red";
        printf("  edge %d: (%u - %u) %s\n", e, edge.u, edge.v, col);
    }

    uint64_t high = (uint64_t)(p->live_mask >> 64);
    uint64_t low  = (uint64_t)(p->live_mask);
    if (high > 0) {
        printf("live_mask = 0x%016llx%016llx\n\n", (unsigned long long)high, (unsigned long long)low);
    } else {
        printf("live_mask = 0x%016llx\n\n", (unsigned long long)low);
    }
}
