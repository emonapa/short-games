#ifndef MEMORY_H
#define MEMORY_H

#include <unistd.h>

size_t get_nearest_power_of_2(size_t target_elems);
size_t get_size_free_memory();

#endif // MEMORY_H
