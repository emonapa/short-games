#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>

#include "utils.h"

#include "dyadics.h"
#include "hash_game.h"

static Dyadic reduce_dyadic(Dyadic x) {
    if (x.num == 0) {
        x.exp = 0;
        return x;
    }

    if (x.exp < 0) {
        int sh = -x.exp;
        x.num <<= sh;
        x.exp = 0;
        return x;
    }

    while (x.exp > 0 && (x.num & 1LL) == 0) {
        x.num >>= 1;
        x.exp--;
    }

    return x;
}

Dyadic dyadic_make(long long num, int exp) {
    Dyadic x;
    x.num = num;
    x.exp = exp;
    return reduce_dyadic(x);
}

int dyadic_cmp(Dyadic a, Dyadic b) {
    int E = (a.exp > b.exp) ? a.exp : b.exp;

    long long an = (long long)a.num << (E - a.exp);
    long long bn = (long long)b.num << (E - b.exp);

    if (an < bn) return -1;
    if (an > bn) return 1;
    return 0;
}


Dyadic dyadic_simplest_between(Dyadic a, Dyadic b) {
    if (dyadic_cmp(a, b) >= 0) {
        fprintf(stderr, "simplest_between: interval empty or reversed\n");
        exit(1);
    }

    const int MAX_K = 60;

    //printf("-------- new cycle -----------\n");
    for (int k = 0; k <= MAX_K; ++k) {
        // hledame n tak, aby a < n/2^k < b
        // tj. a*2^k < n < b*2^k
        long long n_min = floor_scaled(a, k) + 1;
        long long n_max = ceil_scaled(b, k) - 1;
        //printf("[k: %d] n_min: %lld, n_max: %lld\n", k, n_min, n_max);

        if (n_min <= n_max) {
            Dyadic x = { (long long)n_min, k };
            return reduce_dyadic(x);
        }
    }

    fprintf(stderr, "simplest_between: no dyadic found within limits\n");
    exit(1);
}


Dyadic dyadic_simplest_above(Dyadic a) {
    if (a.exp == 0) return dyadic_make(a.num + 1, 0);


    if (a.exp >= 62) {
        fprintf(stderr, "dyadic_simplest_above: exp too large\n");
        exit(1);
    }

    long long one = 1LL << a.exp;
    long long q;

    if (a.num >= 0) q = (a.num + one - 1) >> a.exp;
    else q = a.num >> a.exp;

    Dyadic cand = dyadic_make(q, 0);
    if (dyadic_cmp(cand, a) <= 0) cand.num += 1;

    return cand;
}

Dyadic dyadic_simplest_below(Dyadic b) {
    if (b.exp == 0) return dyadic_make(b.num - 1, 0);

    if (b.exp >= 62) {
        fprintf(stderr, "dyadic_simplest_below: exp too large\n");
        exit(1);
    }

    long long q = b.num >> b.exp;
    Dyadic cand = dyadic_make(q, 0);

    if (dyadic_cmp(cand, b) >= 0) cand.num -= 1;

    return cand;
}

double dyadic_to_double(Dyadic a) {
    return (a.num / pow(2, a.exp));
}


// Stack frame pro iterativní výpočet hodnoty
typedef struct {
    edge_mask_t live_mask;
    int      stage;            // 0 = nový, 1 = procházím tahy, 2 = hotovo
    int      edge_index;       // kde jsme v procházení hran
    EdgeColor last_move_color; // barva hrany, kterou jsme šli do potomka

    int      has_left;
    int      has_right;
    Dyadic   l_max;            // max hodnota z levých možností (modré tahy)
    Dyadic   r_min;            // min hodnota z pravých možností (červené tahy)

    Dyadic   result;           // hotová hodnota pro tuto pozici
} SolverFrame;

void solver_initialize(const BaseGraph *g) {
    hash_game_init();
}

// Iterativní výpočet hodnoty pozice (bez rekurze)
Dyadic solve(const BaseGraph *g, edge_mask_t live_mask) {
    SolverFrame stack[MAX_EDGES + 1];
    int top = 0;

    stack[top++] = (SolverFrame){ .live_mask = live_mask, .stage = 0 };

    while (top > 0) {
        SolverFrame *f = &stack[top - 1];

        // -- Stage 0: Memo / triviální případ --
        if (f->stage == 0) {
            Dyadic v;
            if (hash_game_lookup(f->live_mask, &v)) { f->result = v; f->stage = 2; }
            else if (!f->live_mask)         { f->result = dyadic_make(0, 0); f->stage = 2; }
            else                             f->stage = 1;
        }

        // -- Stage 1: Procházení hran --
        if (f->stage == 1) {
            int pushed = 0;
            for (int e = f->edge_index; e < g->num_edges; e++) {
                if (!IS_BIT_ACTIVE(f->live_mask, e)) continue;

                f->edge_index      = e + 1;
                f->last_move_color = g->edges[e].color;
                edge_mask_t child     = cleanup_position(g, RESET_BIT_AT(f->live_mask, e));

                stack[top++] = (SolverFrame){ .live_mask = child, .stage = 0 };
                pushed = 1;
                break;
            }

            if (!pushed) {
                Dyadic r;
                if      (!f->has_left && !f->has_right)       r = dyadic_make(0, 0);
                else if ( f->has_left && !f->has_right)       r = dyadic_simplest_above(f->l_max);
                else if (!f->has_left &&  f->has_right)       r = dyadic_simplest_below(f->r_min);
                else if (dyadic_cmp(f->l_max, f->r_min) < 0) r = dyadic_simplest_between(f->l_max, f->r_min);
                else                                          r = f->l_max;
                f->result = r;
                f->stage  = 2;
            }
        }

        // -- Stage 2: Propagace výsledku rodiči --
        if (f->stage == 2) {
            Dyadic value = f->result;
            hash_game_insert(f->live_mask , value);
            top--;
            if (top == 0) return value;

            SolverFrame *p = &stack[top - 1];
            if (p->last_move_color == EDGE_BLUE) {
                if (!p->has_left  || dyadic_cmp(value, p->l_max) > 0) { p->l_max = value; p->has_left  = 1; }
            } else {
                if (!p->has_right || dyadic_cmp(value, p->r_min) < 0) { p->r_min = value; p->has_right = 1; }
            }
        }
    }

    return dyadic_make(0, 0);
}

/*
Dyadic solve(const BaseGraph *g, edge_mask_t live_mask) {
    PositionKey key = { live_mask };
    Dyadic memo;
    if (hash_game_lookup(&key, &memo)) return memo;
    if (!live_mask) return dyadic_make(0, 0);

    int has_left = 0, has_right = 0;
    Dyadic l_max, r_min;

    for (int e = 0; e < g->num_edges; e++) {
        if (!IS_BIT_ACTIVE(live_mask, e)) continue;

        edge_mask_t child = cleanup_position(g, RESET_BIT_AT(live_mask, e));
        Dyadic val = solve(g, child);

        if (g->edges[e].color == EDGE_BLUE) {
            if (!has_left  || dyadic_cmp(val, l_max) > 0) { l_max = val; has_left  = 1; }
        } else {
            if (!has_right || dyadic_cmp(val, r_min) < 0) { r_min = val; has_right = 1; }
        }
    }

    Dyadic result;
    if      (!has_left && !has_right)              result = dyadic_make(0, 0);
    else if ( has_left && !has_right)              result = dyadic_simplest_above(l_max);
    else if (!has_left &&  has_right)              result = dyadic_simplest_below(r_min);
    else if (dyadic_cmp(l_max, r_min) < 0)        result = dyadic_simplest_between(l_max, r_min);
    else                                           result = l_max;

    hash_game_insert(&key, result);
    return result;
}
*/
