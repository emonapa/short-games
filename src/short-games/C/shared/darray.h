/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

/*
 * Dynamic array implementation using the "stretchy buffer" technique.
 *
 * The array pointer points directly to the first element. Metadata such as
 * length and capacity are stored in a small header placed immediately before
 * the returned data pointer.
 *
 * This implementation is custom for this project. The general technique is
 * inspired by Sean Barrett's stretchy_buffer.h from the stb libraries.
 *
 * Reference:
 *   Sean Barrett, stretchy_buffer.h, stb single-file public domain libraries
 *   https://github.com/nothings/stb
 */

#ifndef DARRAY_H
#define DARRAY_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

typedef struct DArrayHeader {
    size_t len;
    size_t cap;
} DArrayHeader;

#define da_header(a) \
    ((DArrayHeader *)((char *)(a) - sizeof(DArrayHeader)))

#define da_len(a) \
    ((a) ? da_header(a)->len : 0)

#define da_cap(a) \
    ((a) ? da_header(a)->cap : 0)

#define da_free(a)                     \
    do {                               \
        if (a) {                       \
            free(da_header(a));        \
            (a) = NULL;                \
        }                              \
    } while (0)

#define da_reserve(a, expected_cap)                                      \
    do {                                                                \
        if ((expected_cap) > da_cap(a)) {                               \
            (a) = da_grow((a), sizeof(*(a)), (expected_cap));           \
        }                                                               \
    } while (0)

#define da_push(a, value)                                                \
    do {                                                                 \
        if (da_len(a) >= da_cap(a)) {                                    \
            (a) = da_grow((a), sizeof(*(a)), da_len(a) + 1);             \
        }                                                                \
        (a)[da_header(a)->len++] = (value);                              \
    } while (0)

#define da_append(a, value) \
    da_push((a), (value))

#define da_append_many(a, other)                                         \
    do {                                                                 \
        size_t da_other_len = da_len(other);                             \
        if (da_other_len > 0) {                                          \
            size_t da_old_len = da_len(a);                               \
            da_reserve((a), da_old_len + da_other_len);                  \
            memcpy((a) + da_old_len,                                     \
                   (other),                                              \
                   da_other_len * sizeof(*(a)));                         \
            da_header(a)->len = da_old_len + da_other_len;               \
        }                                                                \
    } while (0)

#define da_resize(a, new_len)          \
    do {                               \
        da_reserve((a), (new_len));    \
        da_header(a)->len = (new_len); \
    } while (0)

#define da_pop(a) \
    ((a)[--da_header(a)->len])

#define da_first(a) \
    ((a)[0])

#define da_last(a) \
    ((a)[da_len(a) - 1])

#define da_remove_unordered(a, index)                                    \
    do {                                                                 \
        size_t da_i = (index);                                           \
        size_t da_n = da_len(a);                                         \
        (a)[da_i] = (a)[da_n - 1];                                       \
        da_header(a)->len = da_n - 1;                                   \
    } while (0)

#define da_foreach(Type, it, a) \
    for (Type *it = (a); it < (a) + da_len(a); ++it)

static void *da_grow(void *arr, size_t elem_size, size_t min_cap) {
    size_t old_len = arr ? da_header(arr)->len : 0;
    size_t old_cap = arr ? da_header(arr)->cap : 0;
    size_t new_cap = old_cap ? old_cap * 2 : 8;

    while (new_cap < min_cap) {
        new_cap *= 2;
    }

    size_t new_size = sizeof(DArrayHeader) + new_cap * elem_size;

    DArrayHeader *new_header;
    if (arr) {
        new_header = realloc(da_header(arr), new_size);
        if (new_header == NULL) warning("Error while reallocating dynamic array with size %zu.\n", new_size);
    } else {
        new_header = malloc(new_size);
        if (new_header == NULL) warning("Error while allocating dynamic array with size %zu.\n", new_size);
    }

    if (new_header == NULL) return NULL;

    new_header->len = old_len;
    new_header->cap = new_cap;

    return (char *)new_header + sizeof(DArrayHeader);
}

#endif
