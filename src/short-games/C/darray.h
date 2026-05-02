#ifndef DYN_ARRAY_H
#define DYN_ARRAY_H

#include <stdlib.h>
#include <stddef.h>

#include "error.h"

typedef struct DynArrayHeader {
    size_t len;
    size_t cap;
} DynArrayHeader;

#define da_header(a) \
    ((DynArrayHeader *)((char *)(a) - sizeof(DynArrayHeader)))

#define da_len(a) \
    ((a) ? da_header(a)->len : 0)

#define da_cap(a) \
    ((a) ? da_header(a)->cap : 0)

#define da_free(a) \
    do { \
        if (a) { \
            free(da_header(a)); \
            (a) = NULL; \
        } \
    } while (0)

#define da_push(a, value) \
    do { \
        if (da_len(a) >= da_cap(a)) { \
            (a) = da_grow((a), sizeof(*(a))); \
        } \
        (a)[da_header(a)->len++] = (value); \
    } while (0)

static void *da_grow(void *arr, size_t elem_size)
{
    size_t old_len = arr ? da_header(arr)->len : 0;
    size_t old_cap = arr ? da_header(arr)->cap : 0;
    size_t new_cap = old_cap ? old_cap * 2 : 8;

    size_t new_size = sizeof(DynArrayHeader) + new_cap * elem_size;

    DynArrayHeader *new_header;

    if (arr) new_header = realloc(da_header(arr), new_size);
    else new_header     = malloc(new_size);

    if (new_header == NULL) error_exit(ERR_MALLOC, "Error while allocating in dynamic array.");

    new_header->len = old_len;
    new_header->cap = new_cap;

    return (char *)new_header + sizeof(DynArrayHeader);
}

#endif
