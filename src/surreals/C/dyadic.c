#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>

#include "dyadic.h"


static inline __int128 floor_div_pow2_i128(__int128 x, int d) {
    // floor(x / 2^d), d >= 0
    if (d <= 0) return x;
    __int128 denom = ((__int128)1) << d;
    __int128 q = x / denom;   // v C je to trunc toward 0
    __int128 r = x % denom;
    if (r != 0 && x < 0) q -= 1; // dorovnat na floor pro zaporna
    return q;
}

static inline __int128 ceil_div_pow2_i128(__int128 x, int d) {
    // ceil(x / 2^d)
    if (d <= 0) return x;
    __int128 denom = ((__int128)1) << d;
    __int128 q = x / denom;
    __int128 r = x % denom;
    if (r != 0 && x > 0) q += 1; // dorovnat na ceil pro kladna
    return q;
}


static inline __int128 floor_scaled(Dyadic a, int k) {
    // floor(a * 2^k)
    __int128 num = (__int128)a.num;
    if (k >= a.exp) {
        return num << (k - a.exp);
    } else {
        return floor_div_pow2_i128(num, a.exp - k);
    }
}

static inline __int128 ceil_scaled(Dyadic a, int k) {
    // ceil(a * 2^k)
    __int128 num = (__int128)a.num;
    if (k >= a.exp) {
        return num << (k - a.exp);
    } else {
        return ceil_div_pow2_i128(num, a.exp - k);
    }
}



static Dyadic reduce_dyadic(Dyadic x) {
    if (x.num == 0) {
        x.exp = 0;
        return x;
    }

    if (x.exp < 0) {
        int sh = -x.exp;
        if (sh >= 63) {
            fprintf(stderr, "reduce_dyadic: exp too negative\n");
            exit(1);
        }

        if (x.num > 0) {
            if (x.num > (LLONG_MAX >> sh)) {
                fprintf(stderr, "reduce_dyadic: overflow\n");
                exit(1);
            }
        } else {
            if (x.num < (LLONG_MIN >> sh)) {
                fprintf(stderr, "reduce_dyadic: overflow\n");
                exit(1);
            }
        }

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

    __int128 an = (__int128)a.num << (E - a.exp);
    __int128 bn = (__int128)b.num << (E - b.exp);

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
        __int128 n_min = floor_scaled(a, k) + 1;
        __int128 n_max = ceil_scaled(b, k) - 1;
        //printf("[k: %d] n_min: %lld, n_max: %lld\n", k, n_min, n_max);

        if (n_min <= n_max) {
            if (n_min < LLONG_MIN || n_min > LLONG_MAX) continue;
            Dyadic x = { (long long)n_min, k };
            return reduce_dyadic(x);
        }
    }

    fprintf(stderr, "simplest_between: no dyadic found within limits\n");
    exit(1);
}


Dyadic dyadic_simplest_above(Dyadic a) {
    if (a.exp == 0) {
        return dyadic_make(a.num + 1, 0);
    }

    if (a.exp >= 62) {
        fprintf(stderr, "dyadic_simplest_above: exp too large\n");
        exit(1);
    }

    long long one = 1LL << a.exp;
    long long q;

    if (a.num >= 0) {
        q = (a.num + one - 1) >> a.exp;
    } else {
        q = a.num >> a.exp;
    }

    Dyadic cand = dyadic_make(q, 0);
    if (dyadic_cmp(cand, a) <= 0) {
        cand.num += 1;
    }

    return cand;
}

Dyadic dyadic_simplest_below(Dyadic b) {
    if (b.exp == 0) {
        return dyadic_make(b.num - 1, 0);
    }

    if (b.exp >= 62) {
        fprintf(stderr, "dyadic_simplest_below: exp too large\n");
        exit(1);
    }

    long long q = b.num >> b.exp;
    Dyadic cand = dyadic_make(q, 0);

    if (dyadic_cmp(cand, b) >= 0) {
        cand.num -= 1;
    }

    return cand;
}

double dyadic_to_double(Dyadic a) {
    return (a.num / pow(2, a.exp));
}
