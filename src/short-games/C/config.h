/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hackenbushe s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hackenbush Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <assert.h>

#define MAX_ITEMS(table_size) ((size_t)(0.85 * table_size))
#define PROBE_LIMIT 128
static_assert(PROBE_LIMIT > 0);

#define PCT_GEQ    0.94
#define PCT_ADD    0.005
#define PCT_CANON  0.065
#define PCT_INTERN 0.08 // tak vysoka, protoze bez teto cache pocitani konci

#define PCT_POS    0.04

// cache sizes
extern size_t canon_items_count;
extern size_t intern_items_count;
extern size_t add_items_count;
extern size_t geq_items_count;
extern size_t pos_items_count;

// other meta information
extern int cannon_count;

#endif // CONFIG_H
