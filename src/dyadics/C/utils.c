#include "dyadics.h"
#include "raw_game.h"

long long floor_div_pow2_i128(long long x, int d) {
    // floor(x / 2^d), d >= 0
    if (d <= 0) return x;
    long long denom = BIT(d);
    long long q = x / denom;   // v C je to trunc toward 0
    long long r = x % denom;
    if (r != 0 && x < 0) q -= 1; // dorovnat na floor pro zaporna
    return q;
}

long long ceil_div_pow2_i128(long long x, int d) {
    // ceil(x / 2^d)
    if (d <= 0) return x;
    long long denom = BIT(d);
    long long q = x / denom;
    long long r = x % denom;
    if (r != 0 && x > 0) q += 1; // dorovnat na ceil pro kladna
    return q;
}

long long floor_scaled(Dyadic a, int k) {
    // floor(a * 2^k)
    long long num = (long long)a.num;
    if (k >= a.exp) {
        return num << (k - a.exp);
    } else {
        return floor_div_pow2_i128(num, a.exp - k);
    }
}

long long ceil_scaled(Dyadic a, int k) {
    // ceil(a * 2^k)
    long long num = (long long)a.num;
    if (k >= a.exp) {
        return num << (k - a.exp);
    } else {
        return ceil_div_pow2_i128(num, a.exp - k);
    }
}
