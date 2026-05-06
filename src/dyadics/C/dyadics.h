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

// a < b, najdi "nejjednodušší" dyadické číslo přísně mezi.
Dyadic dyadic_simplest_between(Dyadic a, Dyadic b);
Dyadic dyadic_simplest_above(Dyadic a);
Dyadic dyadic_simplest_below(Dyadic b);

// Inicializuje hash, může si přednaplnit známé pozice
void solver_initialize(const BaseGraph *g);

// Přesná hodnota pozice dané maskou hran (ignoruje player_to_move).
Dyadic solve(const BaseGraph *g, edge_mask_t live_mask);

#endif // SURREALS_H
