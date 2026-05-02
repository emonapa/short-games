#ifndef DOMINEERING_H
#define DOMINEERING_H

#include <stdint.h>

#define DOMINEERING_MAX_WIDTH 8
#define DOMINEERING_MAX_HEIGHT 8
#define DOMINEERING_MAX_CELLS 64

typedef struct {
    uint8_t width;
    uint8_t height;
} DomineeringBoard;

typedef struct {
    uint64_t occupied_mask;
} DomineeringPosition;

DomineeringBoard domineering_make_board(uint8_t width, uint8_t height);
DomineeringPosition domineering_make_position(uint64_t occupied_mask);

#endif
