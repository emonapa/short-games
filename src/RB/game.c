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

// Provede tah: smaže hranu e, udělá cleanup a přepne hráče
Position do_move(const BaseGraph *g, const Position *p, int edge_index) {
    Position np = *p;
    np.live_mask &= ~(1ULL << edge_index);     // smažu hranu
    np.live_mask = cleanup_position(g, np.live_mask); // cleanup
    np.player_to_move ^= 1;                    // druhý hráč
    return np;
}


void print_position(const BaseGraph *g, const Position *p) {
    printf("Pozice: player_to_move = %u\n", p->player_to_move);
    printf("Zive hrany:\n");
    for (int e = 0; e < g->num_edges; ++e) {
        if (!(p->live_mask & (1ULL << e))) continue;
        Edge edge = g->edges[e];
        const char *col = (edge.color == EDGE_BLUE) ? "blue" : "red";
        printf("  edge %d: (%u - %u) %s\n", e, edge.u, edge.v, col);
    }
    printf("live_mask = 0x%016llx\n\n", (unsigned long long)p->live_mask);
}


void export_graph_svg(const BaseGraph *g, const Position *p, const char *filename) {
    if (!g || g->num_vertices == 0 || !filename) return;

    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        return;
    }

    // BFS vzdalenosti od vrcholu 0
    uint8_t dist[MAX_VERTICES];
    for (int i = 0; i < MAX_VERTICES; ++i) {
        dist[i] = 0xFF; // "nekonecno"
    }

    uint8_t queue[MAX_VERTICES];
    uint8_t qh = 0, qt = 0;

    dist[0] = 0;
    queue[qt++] = 0;

    while (qh < qt) {
        uint8_t v = queue[qh++];
        for (int e = 0; e < g->num_edges; ++e) {
            // pokud pouzivame pozici, hranu bereme jen kdyz je ziva
            if (p && !(p->live_mask & (1ULL << e))) continue;

            uint8_t u = 0xFF;
            if (g->edges[e].u == v) u = g->edges[e].v;
            else if (g->edges[e].v == v) u = g->edges[e].u;
            else continue;

            if (dist[u] == 0xFF) {
                dist[u] = (uint8_t)(dist[v] + 1);
                queue[qt++] = u;
            }
        }
    }

    // max vyska (nejvetsi vzdalenost od zeme)
    uint8_t max_dist = 0;
    for (int v = 0; v < g->num_vertices; ++v) {
        if (dist[v] != 0xFF && dist[v] > max_dist) {
            max_dist = dist[v];
        }
    }

    // kolik vrcholu v jednotlivych vrstvach
    uint8_t level_count[MAX_VERTICES] = {0};
    uint8_t level_index[MAX_VERTICES] = {0};

    for (int v = 0; v < g->num_vertices; ++v) {
        if (dist[v] == 0xFF) continue; // nepropojene se zemi
        uint8_t d = dist[v];
        level_index[v] = level_count[d];
        level_count[d]++;
    }

    uint8_t max_per_level = 0;
    for (int d = 0; d <= max_dist; ++d) {
        if (level_count[d] > max_per_level) {
            max_per_level = level_count[d];
        }
    }

    if (max_per_level == 0) {
        fprintf(stderr, "Graf nema vrcholy propojene se zemi.\n");
        fclose(f);
        return;
    }

    // layout parametry
    const double margin = 40.0;
    const double h_spacing = 80.0;
    const double v_spacing = 100.0;
    const double node_r = 12.0;

    int width  = (int)(margin * 2 + (max_per_level - 1) * h_spacing + 2 * node_r);
    int height = (int)(margin * 2 + max_dist * v_spacing + 2 * node_r);

    // pozice vrcholu v "SVG souradnicich"
    double vx[MAX_VERTICES];
    double vy[MAX_VERTICES];

    for (int v = 0; v < g->num_vertices; ++v) {
        if (dist[v] == 0xFF) {
            vx[v] = vy[v] = -1000.0; // mimo obraz
            continue;
        }

        uint8_t d = dist[v];
        uint8_t idx = level_index[v];

        double x = margin + idx * h_spacing;
        double y = margin + (max_dist - d) * v_spacing; // zem dole (dist 0)

        vx[v] = x;
        vy[v] = y;
    }

    // SVG hlavicka
    fprintf(f,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" "
        "viewBox=\"0 0 %d %d\">\n",
        width, height, width, height);

    // pozadi
    fprintf(f,
        "  <rect x=\"0\" y=\"0\" width=\"%d\" height=\"%d\" fill=\"white\" />\n",
        width, height);

    // zem - horizontalni cara pres spodni vrstvu (kolem vrcholu 0)
    if (dist[0] != 0xFF) {
        double ground_y = vy[0] + node_r + 10.0;
        fprintf(f,
            "  <line x1=\"0\" y1=\"%.1f\" x2=\"%d\" y2=\"%.1f\" "
            "stroke=\"black\" stroke-width=\"3\" />\n",
            ground_y, width, ground_y);
    }

    // hrany
    for (int e = 0; e < g->num_edges; ++e) {
        if (p && !(p->live_mask & (1ULL << e))) continue; // jen zive hrany (pokud je pozice dana)

        uint8_t u = g->edges[e].u;
        uint8_t v = g->edges[e].v;
        if (u >= g->num_vertices || v >= g->num_vertices) continue;
        if (dist[u] == 0xFF || dist[v] == 0xFF) continue;

        const char *color = (g->edges[e].color == EDGE_BLUE) ? "blue" : "red";

        fprintf(f,
            "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" "
            "stroke=\"%s\" stroke-width=\"4\" />\n",
            vx[u], vy[u], vx[v], vy[v], color);
    }

    // vrcholy
    for (int v = 0; v < g->num_vertices; ++v) {
        if (dist[v] == 0xFF) continue;

        fprintf(f,
            "  <circle cx=\"%.1f\" cy=\"%.1f\" r=\"%.1f\" "
            "fill=\"white\" stroke=\"black\" stroke-width=\"2\" />\n",
            vx[v], vy[v], node_r);

        // label s indexem vrcholu
        fprintf(f,
            "  <text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\" "
            "dominant-baseline=\"middle\" font-family=\"monospace\" "
            "font-size=\"12\">%d</text>\n",
            vx[v], vy[v], v);
    }

    // legenda barev
    fprintf(f,
        "  <rect x=\"10\" y=\"10\" width=\"12\" height=\"12\" fill=\"blue\" />\n"
        "  <text x=\"28\" y=\"20\" font-family=\"monospace\" font-size=\"12\">BLUE</text>\n"
        "  <rect x=\"10\" y=\"30\" width=\"12\" height=\"12\" fill=\"red\" />\n"
        "  <text x=\"28\" y=\"40\" font-family=\"monospace\" font-size=\"12\">RED</text>\n");

    fprintf(f, "</svg>\n");
    fclose(f);
}
