#ifndef CONFIG_H
#define CONFIG_H

#define MAX_ITEMS(table_size) ((uint32_t)(0.75 * table_size))
#define PROBE_LIMIT 128

#define PCT_GEQ    0.94
#define PCT_ADD    0.005
#define PCT_CANON  0.065
#define PCT_INTERN 0.08 // tak vysoka, protoze bez teto cache pocitani konci

#define PCT_POS    0.04

extern int cannon_count;

extern size_t canon_items_count;
extern size_t intern_items_count;
extern size_t add_items_count;
extern size_t geq_items_count;
extern size_t pos_items_count;

extern size_t stack_items_count;

#endif // CONFIG_H
