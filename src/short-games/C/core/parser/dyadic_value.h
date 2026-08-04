/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef DYADIC_VALUE_H
#define DYADIC_VALUE_H

#include "../short_game.h"

#ifdef __SIZEOF_INT128__
    typedef __int128 dyadic_precision_t;
    typedef unsigned __int128 dyadic_unsigned_precision_t;
#else
    typedef int64_t dyadic_precision_t;
    typedef uint64_t dyadic_unsigned_precision_t;
#endif

#define DYADIC_PRECISION_BITS \
    ((unsigned)(sizeof(dyadic_precision_t) * CHAR_BIT))
#define BIT(exponent) \
    (((dyadic_unsigned_precision_t)1) << (exponent))
#define DYADIC_PRECISION_MAX \
    ((dyadic_precision_t)(BIT(DYADIC_PRECISION_BITS - 1u) - 1u))
#define DYADIC_PRECISION_MIN (-DYADIC_PRECISION_MAX - 1)


typedef enum DyadicDivisionStatus {
    DYADIC_DIVISION_OK = 0,
    DYADIC_DIVISION_NOT_NUMBERS,
    DYADIC_DIVISION_BY_ZERO,
    DYADIC_DIVISION_NON_DYADIC,
    DYADIC_DIVISION_OVERFLOW,
    DYADIC_DIVISION_OUT_OF_RANGE
} DyadicDivisionStatus;

/**
 * @brief Divides two numeric games and returns arguments for make_dyadic().
 *
 * Values are represented exactly as n/2^k. Game values are obtained with the
 * same "simplest number between options" construction used by the dyadics
 * solver, so this does not depend on floating-point conversion.
 */
DyadicDivisionStatus dyadic_divide_games(Game *numerator,
                                         Game *denominator,
                                         int *out_numerator,
                                         int *out_denominator);

#endif // DYADIC_VALUE_H
