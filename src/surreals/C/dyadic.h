#ifndef DYADIC_H
#define DYADIC_H

#include <stdint.h>

typedef struct {
    long long num; // num / 2^exp
    int exp;
} Dyadic;


/* Základní operace */
Dyadic dyadic_make(long long num, int exp);
int dyadic_cmp(Dyadic a, Dyadic b);
double dyadic_to_double(Dyadic a);

/*
 a < b, najdi "nejjednodušší" dyadické číslo přísně mezi.
*/
Dyadic dyadic_simplest_between(Dyadic a, Dyadic b);
Dyadic dyadic_simplest_above(Dyadic a);
Dyadic dyadic_simplest_below(Dyadic b);

#endif // DYADIC_H
