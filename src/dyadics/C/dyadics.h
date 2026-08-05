#ifndef SURREALS_H
#define SURREALS_H

#include <stdint.h>
#include "raw_game.h"

typedef struct {
    long long num; // num / 2^exp
    int exp;
} Dyadic;

Dyadic dyadic_make(long long num, int exp);
int dyadic_cmp(Dyadic a, Dyadic b);
double dyadic_to_double(Dyadic a);

// For a < b, returns the simplest dyadic number strictly between them.
Dyadic dyadic_simplest_between(Dyadic a, Dyadic b);
Dyadic dyadic_simplest_above(Dyadic a);
Dyadic dyadic_simplest_below(Dyadic b);

// Initializes the hash table and may preload known positions.
void solver_initialize(const BaseGraph *g);

// Returns the exact value of an edge-mask position, ignoring player_to_move.
Dyadic solve(const BaseGraph *g, edge_mask_t live_mask);

#endif // SURREALS_H
