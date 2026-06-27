#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <unistd.h>

#include "shared/error.h"

static inline size_t get_nearest_power_of_2(size_t target_elems) {
    if (target_elems == 0) {
        warning("Trying to get nearest power of two on 0\n");
    }

    size_t size = 1;

    while ((size << 1) <= target_elems) {
        size <<= 1;
    }

    return size;
}

static inline size_t get_size_free_memory(void) {
    long pages = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);

    if (pages == -1 || page_size == -1) {
        // fallback na 2 GB
        return 2ULL * 1024 * 1024 * 1024;
    }

    return (size_t)pages * (size_t)page_size;
}

#endif // MEMORY_H
