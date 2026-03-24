#include <stdio.h>
#include <stdint.h>

#include "game.h"

// Pomocná funkce pro adjacency list
void build_adjacency(const BaseGraph *g, uint64_t live_mask,
                            uint8_t *deg, uint8_t adj[][MAX_EDGES]) {
    for (int i = 0; i < g->num_vertices; ++i) {
        deg[i] = 0;
    }

    for (int e = 0; e < g->num_edges; ++e) {
        if (!(live_mask & (1ULL << e))) continue;
        uint8_t u = g->edges[e].u;
        uint8_t v = g->edges[e].v;
        adj[u][deg[u]++] = v;
        adj[v][deg[v]++] = u;
    }
}

// Vyhodí všechny komponenty nepropojené se zemí (vrchol 0)
uint64_t cleanup_position(const BaseGraph *g, uint64_t live_mask) {
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
        if (!(live_mask & (1ULL << e))) continue;
        uint8_t u = g->edges[e].u;
        uint8_t v = g->edges[e].v;
        if (reachable[u] && reachable[v]) {
            new_mask |= (1ULL << e);
        }
    }
    return new_mask;
}
