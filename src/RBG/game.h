#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define MAX_VERTICES  129
#define MAX_EDGES     128

typedef unsigned __int128 edge_mask_t;

// Bezpečný posun pro uint128
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

typedef struct {
    edge_mask_t live_mask;      // bity hran
    uint8_t  player_to_move; // 0 = modrý, 1 = červený
} Position;

void build_adjacency(const BaseGraph *g, edge_mask_t live_mask,
                            uint8_t *deg, uint8_t adj[][MAX_EDGES]);

edge_mask_t cleanup_position(const BaseGraph *g, edge_mask_t live_mask);

Position do_move(const BaseGraph *g, const Position *p, int edge_index);

void print_position(const BaseGraph *g, const Position *p);


#endif // GAME_H
