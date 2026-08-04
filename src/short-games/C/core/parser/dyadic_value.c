/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include "dyadic_value.h"

#include <limits.h>
#include <stdint.h>

#include "../game_darray.h"

#define MAX_EXACT_EXPONENT 62u

/*
 * Keep exact intermediate arithmetic where the compiler supports it, while
 * retaining a portable, checked 64-bit fallback. This mirrors the precision
 * selection used by dyadics/raw_game.h without exposing a compiler extension
 * in the parser's public API.
 */

typedef struct DyadicValue {
    int64_t numerator;
    unsigned exponent;
} DyadicValue;

typedef enum DyadicValueStatus {
    DYADIC_VALUE_OK = 0,
    DYADIC_VALUE_NOT_NUMBER,
    DYADIC_VALUE_OVERFLOW
} DyadicValueStatus;

static void dyadic_normalize(DyadicValue *value) {
    if (value->numerator == 0) {
        value->exponent = 0;
        return;
    }

    while (value->exponent > 0 && value->numerator % 2 == 0) {
        value->numerator /= 2;
        value->exponent--;
    }
}

static DyadicValueStatus scale_numerator(int64_t numerator,
                                         unsigned shift,
                                         dyadic_precision_t *out) {
    if (shift >= DYADIC_PRECISION_BITS - 1u) {
        if (numerator == 0) {
            *out = 0;
            return DYADIC_VALUE_OK;
        }
        return DYADIC_VALUE_OVERFLOW;
    }

    dyadic_precision_t factor = (dyadic_precision_t)BIT(shift);
    dyadic_precision_t value = (dyadic_precision_t)numerator;
    if (value > DYADIC_PRECISION_MAX / factor ||
        value < DYADIC_PRECISION_MIN / factor) {
        return DYADIC_VALUE_OVERFLOW;
    }

    *out = value * factor;
    return DYADIC_VALUE_OK;
}

static DyadicValueStatus dyadic_compare(DyadicValue left,
                                        DyadicValue right,
                                        int *out_comparison) {
    unsigned exponent = left.exponent > right.exponent
                      ? left.exponent
                      : right.exponent;
    dyadic_precision_t left_scaled;
    dyadic_precision_t right_scaled;
    DyadicValueStatus status = scale_numerator(
        left.numerator, exponent - left.exponent, &left_scaled
    );
    if (status != DYADIC_VALUE_OK) return status;

    status = scale_numerator(
        right.numerator, exponent - right.exponent, &right_scaled
    );
    if (status != DYADIC_VALUE_OK) return status;

    *out_comparison = (left_scaled > right_scaled) -
                      (left_scaled < right_scaled);
    return DYADIC_VALUE_OK;
}

static DyadicValueStatus floor_scaled(DyadicValue value,
                                      unsigned exponent,
                                      dyadic_precision_t *out) {
    if (exponent >= value.exponent) {
        return scale_numerator(
            value.numerator, exponent - value.exponent, out
        );
    }

    unsigned shift = value.exponent - exponent;
    if (shift >= DYADIC_PRECISION_BITS - 1u) {
        return DYADIC_VALUE_OVERFLOW;
    }

    dyadic_precision_t divisor = (dyadic_precision_t)BIT(shift);
    dyadic_precision_t quotient = (dyadic_precision_t)value.numerator / divisor;
    dyadic_precision_t remainder = (dyadic_precision_t)value.numerator % divisor;
    if (remainder != 0 && value.numerator < 0) quotient--;
    *out = quotient;
    return DYADIC_VALUE_OK;
}

static DyadicValueStatus ceil_scaled(DyadicValue value,
                                     unsigned exponent,
                                     dyadic_precision_t *out) {
    if (exponent >= value.exponent) {
        return scale_numerator(
            value.numerator, exponent - value.exponent, out
        );
    }

    unsigned shift = value.exponent - exponent;
    if (shift >= DYADIC_PRECISION_BITS - 1u) {
        return DYADIC_VALUE_OVERFLOW;
    }

    dyadic_precision_t divisor = (dyadic_precision_t)BIT(shift);
    dyadic_precision_t quotient = (dyadic_precision_t)value.numerator / divisor;
    dyadic_precision_t remainder = (dyadic_precision_t)value.numerator % divisor;
    if (remainder != 0 && value.numerator > 0) quotient++;
    *out = quotient;
    return DYADIC_VALUE_OK;
}

static DyadicValueStatus simplest_above(DyadicValue lower,
                                        DyadicValue *out) {
    dyadic_precision_t integer;
    if (lower.exponent == 0) {
        if (lower.numerator == INT64_MAX) return DYADIC_VALUE_OVERFLOW;
        integer = (dyadic_precision_t)lower.numerator + 1;
    } else {
        DyadicValueStatus status = ceil_scaled(lower, 0, &integer);
        if (status != DYADIC_VALUE_OK) return status;
    }

    if (integer < INT64_MIN || integer > INT64_MAX) {
        return DYADIC_VALUE_OVERFLOW;
    }

    *out = (DyadicValue){ .numerator = (int64_t)integer, .exponent = 0 };
    return DYADIC_VALUE_OK;
}

static DyadicValueStatus simplest_below(DyadicValue upper,
                                        DyadicValue *out) {
    dyadic_precision_t integer;
    if (upper.exponent == 0) {
        if (upper.numerator == INT64_MIN) return DYADIC_VALUE_OVERFLOW;
        integer = (dyadic_precision_t)upper.numerator - 1;
    } else {
        DyadicValueStatus status = floor_scaled(upper, 0, &integer);
        if (status != DYADIC_VALUE_OK) return status;
    }

    if (integer < INT64_MIN || integer > INT64_MAX) {
        return DYADIC_VALUE_OVERFLOW;
    }

    *out = (DyadicValue){ .numerator = (int64_t)integer, .exponent = 0 };
    return DYADIC_VALUE_OK;
}

static DyadicValueStatus simplest_between(DyadicValue lower,
                                          DyadicValue upper,
                                          DyadicValue *out) {
    int comparison;
    DyadicValueStatus status = dyadic_compare(lower, upper, &comparison);
    if (status != DYADIC_VALUE_OK) return status;
    if (comparison >= 0) return DYADIC_VALUE_NOT_NUMBER;

    for (unsigned exponent = 0; exponent <= MAX_EXACT_EXPONENT; exponent++) {
        dyadic_precision_t lower_floor;
        dyadic_precision_t upper_ceil;
        status = floor_scaled(lower, exponent, &lower_floor);
        if (status != DYADIC_VALUE_OK) return status;
        status = ceil_scaled(upper, exponent, &upper_ceil);
        if (status != DYADIC_VALUE_OK) return status;
        if (lower_floor == DYADIC_PRECISION_MAX ||
            upper_ceil == DYADIC_PRECISION_MIN) {
            return DYADIC_VALUE_OVERFLOW;
        }

        dyadic_precision_t first = lower_floor + 1;
        dyadic_precision_t last = upper_ceil - 1;
        if (first > last) continue;
        if (first < INT64_MIN || first > INT64_MAX) {
            return DYADIC_VALUE_OVERFLOW;
        }

        *out = (DyadicValue){
            .numerator = (int64_t)first,
            .exponent = exponent
        };
        dyadic_normalize(out);
        return DYADIC_VALUE_OK;
    }
    return DYADIC_VALUE_OVERFLOW;
}

static DyadicValueStatus dyadic_from_game(Game *game, DyadicValue *out) {
    if (game == NULL || out == NULL) return DYADIC_VALUE_NOT_NUMBER;

    size_t left_count = game_len(&game->left);
    size_t right_count = game_len(&game->right);
    if (left_count == 0 && right_count == 0) {
        *out = (DyadicValue){ .numerator = 0, .exponent = 0 };
        return DYADIC_VALUE_OK;
    }

    DyadicValue left_max = {0};
    DyadicValue right_min = {0};

    for (size_t i = 0; i < left_count; i++) {
        DyadicValue option;
        DyadicValueStatus status = dyadic_from_game(game->left[i], &option);
        if (status != DYADIC_VALUE_OK) return status;
        if (i == 0) {
            left_max = option;
        } else {
            int comparison;
            status = dyadic_compare(option, left_max, &comparison);
            if (status != DYADIC_VALUE_OK) return status;
            if (comparison > 0) left_max = option;
        }
    }

    for (size_t i = 0; i < right_count; i++) {
        DyadicValue option;
        DyadicValueStatus status = dyadic_from_game(game->right[i], &option);
        if (status != DYADIC_VALUE_OK) return status;
        if (i == 0) {
            right_min = option;
        } else {
            int comparison;
            status = dyadic_compare(option, right_min, &comparison);
            if (status != DYADIC_VALUE_OK) return status;
            if (comparison < 0) right_min = option;
        }
    }

    if (left_count == 0) return simplest_below(right_min, out);
    if (right_count == 0) return simplest_above(left_max, out);
    return simplest_between(left_max, right_min, out);
}

static uint64_t magnitude_i64(int64_t value) {
    if (value >= 0) return (uint64_t)value;
    return (uint64_t)(-(value + 1)) + 1u;
}

static uint64_t gcd_u64(uint64_t left, uint64_t right) {
    while (right != 0) {
        uint64_t remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

static int is_power_of_two(uint64_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

static unsigned power_of_two_exponent(uint64_t value) {
    unsigned exponent = 0;
    while (value > 1) {
        value >>= 1;
        exponent++;
    }
    return exponent;
}

static DyadicDivisionStatus divide_values(DyadicValue numerator,
                                          DyadicValue denominator,
                                          DyadicValue *out) {
    if (denominator.numerator == 0) return DYADIC_DIVISION_BY_ZERO;
    if (numerator.numerator == 0) {
        *out = (DyadicValue){ .numerator = 0, .exponent = 0 };
        return DYADIC_DIVISION_OK;
    }

    uint64_t numerator_magnitude = magnitude_i64(numerator.numerator);
    uint64_t denominator_magnitude = magnitude_i64(denominator.numerator);
    uint64_t divisor = gcd_u64(numerator_magnitude, denominator_magnitude);
    numerator_magnitude /= divisor;
    denominator_magnitude /= divisor;

    if (!is_power_of_two(denominator_magnitude)) {
        return DYADIC_DIVISION_NON_DYADIC;
    }

    int exponent = (int)numerator.exponent +
                   (int)power_of_two_exponent(denominator_magnitude) -
                   (int)denominator.exponent;
    int negative = (numerator.numerator < 0) !=
                   (denominator.numerator < 0);
    uint64_t signed_limit = negative
                          ? (uint64_t)INT64_MAX + 1u
                          : (uint64_t)INT64_MAX;

    if (exponent < 0) {
        unsigned shift = (unsigned)-exponent;
        if (shift >= 64 || numerator_magnitude > (signed_limit >> shift)) {
            return DYADIC_DIVISION_OVERFLOW;
        }
        numerator_magnitude <<= shift;
        exponent = 0;
    }

    if (numerator_magnitude > signed_limit) return DYADIC_DIVISION_OVERFLOW;

    int64_t signed_numerator;
    if (!negative) {
        signed_numerator = (int64_t)numerator_magnitude;
    } else if (numerator_magnitude == (uint64_t)INT64_MAX + 1u) {
        signed_numerator = INT64_MIN;
    } else {
        signed_numerator = -(int64_t)numerator_magnitude;
    }

    *out = (DyadicValue){
        .numerator = signed_numerator,
        .exponent = (unsigned)exponent
    };
    dyadic_normalize(out);
    return DYADIC_DIVISION_OK;
}

static DyadicDivisionStatus dyadic_to_make_arguments(DyadicValue value,
                                                      int *out_numerator,
                                                      int *out_denominator) {
    if (value.numerator < INT_MIN || value.numerator > INT_MAX ||
        value.exponent > 30u) {
        return DYADIC_DIVISION_OUT_OF_RANGE;
    }

    *out_numerator = (int)value.numerator;
    *out_denominator = (int)BIT(value.exponent);
    return DYADIC_DIVISION_OK;
}

DyadicDivisionStatus dyadic_divide_games(Game *numerator,
                                         Game *denominator,
                                         int *out_numerator,
                                         int *out_denominator) {
    if (out_numerator == NULL || out_denominator == NULL) {
        return DYADIC_DIVISION_OUT_OF_RANGE;
    }

    DyadicValue left;
    DyadicValue right;
    DyadicValueStatus left_status = dyadic_from_game(numerator, &left);
    DyadicValueStatus right_status = dyadic_from_game(denominator, &right);
    if (left_status == DYADIC_VALUE_NOT_NUMBER ||
        right_status == DYADIC_VALUE_NOT_NUMBER) {
        return DYADIC_DIVISION_NOT_NUMBERS;
    }
    if (left_status == DYADIC_VALUE_OVERFLOW ||
        right_status == DYADIC_VALUE_OVERFLOW) {
        return DYADIC_DIVISION_OVERFLOW;
    }

    DyadicValue quotient;
    DyadicDivisionStatus status = divide_values(left, right, &quotient);
    if (status != DYADIC_DIVISION_OK) return status;
    return dyadic_to_make_arguments(quotient, out_numerator, out_denominator);
}
