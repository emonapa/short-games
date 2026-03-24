#ifndef RAW_GAME_H
#define RAW_GAME_H

#include <stdint.h>

#define MAX_VERTICES  129
#define MAX_EDGES     128

typedef unsigned __int128 edge_mask_t;

// Posun pro uint128
#define BIT(e) (((edge_mask_t)1) << (e))

typedef enum {
    EDGE_BLUE = 0,
    EDGE_RED  = 1,
    EDGE_GREEN = 2
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

void build_adjacency(const BaseGraph *g, edge_mask_t live_mask,
                            uint8_t *deg, uint8_t adj[][MAX_EDGES]);

edge_mask_t cleanup_position(const BaseGraph *g, edge_mask_t live_mask);


#endif // RAW_GAME_H
