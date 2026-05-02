#ifndef RAW_GAME_H
#define RAW_GAME_H

#include <stdint.h>

typedef void *RawGame;
typedef void *Position_t;

int num_moves(RawGame raw_game);

Position_t do_move_left(RawGame raw_game, Position_t position, int e);
Position_t do_move_right(RawGame raw_game, Position_t position, int e);

int can_left_move(RawGame raw_game, Position_t position, int e);
int can_right_move(RawGame raw_game, Position_t position, int e);

uint64_t hash_raw_game_position(RawGame raw_game, Position_t position, int e);

int get_independent_components(RawGame raw_game, Position_t position, Position_t *sub_masks[]);

#endif // RAW_GAME_H
