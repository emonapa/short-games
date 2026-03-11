#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define MAX_VERTICES  32
#define MAX_EDGES     64

typedef enum {
    EDGE_BLUE = 0,
    EDGE_RED  = 1
} EdgeColor;

typedef struct {
    uint8_t u;
    uint8_t v;
    EdgeColor color;
} Edge;

typedef struct {
    uint8_t num_vertices;
    uint8_t num_edges;
    Edge edges[MAX_EDGES];
} BaseGraph;

typedef struct {
    uint64_t live_mask;      // bity hran
    uint8_t  player_to_move; // 0 = modrý, 1 = červený
} Position;

void build_adjacency(const BaseGraph *g, uint64_t live_mask,
                            uint8_t *deg, uint8_t adj[][MAX_EDGES]);

uint64_t cleanup_position(const BaseGraph *g, uint64_t live_mask);

Position do_move(const BaseGraph *g, const Position *p, int edge_index);

void print_position(const BaseGraph *g, const Position *p);

void export_graph_svg(const BaseGraph *g, const Position *p, const char *filename);

#endif // GAME_H
