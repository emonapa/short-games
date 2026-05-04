#ifndef TOADS_AND_FROGS_H
#define TOADS_AND_FROGS_H

#include <stdint.h>

#define TOADS_AND_FROGS_MAX_CELLS 64

typedef struct {
    uint8_t length;
} ToadsAndFrogsBoard;

typedef struct {
    uint64_t toads_mask;
    uint64_t frogs_mask;
} ToadsAndFrogsPosition;

ToadsAndFrogsBoard toads_and_frogs_make_board(uint8_t length);
ToadsAndFrogsPosition toads_and_frogs_make_position(uint64_t toads_mask, uint64_t frogs_mask);

#endif
