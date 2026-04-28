#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "error.h"

size_t get_nearest_power_of_2(size_t target_elems) {
    if (target_elems == 0) warning("Trying to get nearest power of two on 0");
    size_t size = 1;

    while ((size << 1) <= target_elems) {
        size <<= 1;
    }
    return size;
}

size_t get_size_free_memory() {
    long pages = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);

    size_t free_ram_bytes = (size_t)pages * (size_t)page_size;

    //fallback na 2 GB
    if (pages == -1 || page_size == -1) {
        free_ram_bytes = 2ULL * 1024 * 1024 * 1024;
    }

    return free_ram_bytes;
}
