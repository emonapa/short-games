#ifndef CONFIG_H
#define CONFIG_H

#define MAX_ITEMS(table_size) ((uint32_t)(0.75 * table_size))
#define PROBE_LIMIT 128
#define HOW_MUCH_MEM 0.1
//#define HOW_MUCH_MEM 0.8

extern size_t canon_items_count;
extern size_t intern_items_count;
extern size_t add_items_count;
extern size_t geq_items_count;
extern size_t pos_items_count;

extern size_t stack_items_count;

#endif // CONFIG_H
