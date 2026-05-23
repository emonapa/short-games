#ifndef HOTPOTCH_H
#define HOTPOTCH_H

#include <stdint.h>

#ifdef __SIZEOF_INT128__
    typedef unsigned __int128 edge_mask_t;
    #define MAX_EDGES    128
    #define MAX_VERTICES 128
#else
    typedef unsigned uint64_t edge_mask_t;
    #define MAX_EDGES    64
    #define MAX_VERTICES 64
#endif

#define BIT(e) (((edge_mask_t)1) << (e))

#define IS_BIT_ACTIVE(live_mask, e) \
    (((e) >= MAX_EDGES) ? 0 : (int)(((live_mask) >> (e)) & (edge_mask_t)1))

#define SET_BIT_AT(live_mask, e) \
    (((e) < MAX_EDGES) ? ((live_mask) | BIT(e)) : (live_mask))

#define RESET_BIT_AT(live_mask, e) \
    (((e) < MAX_EDGES) ? ((live_mask) & ~BIT(e)) : (live_mask))


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
    edge_mask_t live_mask;
} Position;

void build_adjacency(const BaseGraph *g, edge_mask_t live_mask, uint8_t *deg, uint8_t adj[][MAX_EDGES]);

edge_mask_t cleanup_position(const BaseGraph *g, edge_mask_t live_mask);


#endif // HOTPOTCH_H
