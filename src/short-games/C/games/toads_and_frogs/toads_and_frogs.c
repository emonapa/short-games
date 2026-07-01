#include <stdint.h>
#include <stdlib.h>

#include "../../shared/error.h"
#include "../../shared/darray.h"

#include "toads_and_frogs.h"
#include "raw_game.h"

#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

static uint64_t bit_at(int index) {
    return 1ULL << index;
}

static int is_valid_cell(const ToadsAndFrogsBoard *board, int cell) {
    return cell >= 0 && cell < board->length;
}

static int has_toad(const ToadsAndFrogsPosition *pos, int cell) {
    return (pos->toads_mask & bit_at(cell)) != 0;
}

static int has_frog(const ToadsAndFrogsPosition *pos, int cell) {
    return (pos->frogs_mask & bit_at(cell)) != 0;
}

static int is_empty(const ToadsAndFrogsPosition *pos, int cell) {
    return !has_toad(pos, cell) && !has_frog(pos, cell);
}

static int has_any_move(RawGame_t raw_game, Position_t position) {
    int moves = num_moves(raw_game, position);

    for (int e = 0; e < moves; ++e) {
        if (can_left_move(raw_game, position, e)) {
            return 1;
        }

        if (can_right_move(raw_game, position, e)) {
            return 1;
        }
    }

    return 0;
}

ToadsAndFrogsBoard toads_and_frogs_make_board(uint8_t length) {
    ToadsAndFrogsBoard board;

    if (length == 0) {
        error_exit(ERR_OTHER, "Toads and Frogs board length must be positive.\n");
    }

    if (length > TOADS_AND_FROGS_MAX_CELLS) {
        error_exit(ERR_OTHER, "Toads and Frogs board is too large.\n");
    }

    board.length = length;
    return board;
}

ToadsAndFrogsPosition toads_and_frogs_make_position(uint64_t toads_mask, uint64_t frogs_mask) {
    ToadsAndFrogsPosition pos;

    if ((toads_mask & frogs_mask) != 0) {
        error_exit(ERR_OTHER, "Toads and Frogs position has overlapping pieces.\n");
    }

    pos.toads_mask = toads_mask;
    pos.frogs_mask = frogs_mask;

    return pos;
}

int num_moves(RawGame_t raw_game, Position_t position) {
    if (raw_game == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    ToadsAndFrogsBoard *board = (ToadsAndFrogsBoard *)raw_game;
    return board->length;
}

int can_left_move(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    ToadsAndFrogsBoard *board = (ToadsAndFrogsBoard *)raw_game;
    ToadsAndFrogsPosition *pos = (ToadsAndFrogsPosition *)position;

    if (!is_valid_cell(board, e) || !has_toad(pos, e)) {
        return 0;
    }

    int step = e + 1;
    if (is_valid_cell(board, step) && is_empty(pos, step)) {
        return 1;
    }

    int jump = e + 2;
    if (is_valid_cell(board, jump) && has_frog(pos, step) && is_empty(pos, jump)) {
        return 1;
    }

    return 0;
}

int can_right_move(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    ToadsAndFrogsBoard *board = (ToadsAndFrogsBoard *)raw_game;
    ToadsAndFrogsPosition *pos = (ToadsAndFrogsPosition *)position;

    if (!is_valid_cell(board, e) || !has_frog(pos, e)) {
        return 0;
    }

    int step = e - 1;
    if (is_valid_cell(board, step) && is_empty(pos, step)) {
        return 1;
    }

    int jump = e - 2;
    if (is_valid_cell(board, jump) && has_toad(pos, step) && is_empty(pos, jump)) {
        return 1;
    }

    return 0;
}

Position_t do_move_left(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    ToadsAndFrogsBoard *board = (ToadsAndFrogsBoard *)raw_game;
    ToadsAndFrogsPosition *pos = (ToadsAndFrogsPosition *)position;

    if (!can_left_move(raw_game, position, e)) {
        return NULL;
    }

    int to = e + 1;
    if (!is_valid_cell(board, to) || !is_empty(pos, to)) {
        to = e + 2;
    }

    ToadsAndFrogsPosition *next = malloc(sizeof *next);
    if (next == NULL) {
        return NULL;
    }

    next->toads_mask = pos->toads_mask;
    next->frogs_mask = pos->frogs_mask;

    next->toads_mask &= ~bit_at(e);
    next->toads_mask |= bit_at(to);

    return next;
}

Position_t do_move_right(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    ToadsAndFrogsBoard *board = (ToadsAndFrogsBoard *)raw_game;
    ToadsAndFrogsPosition *pos = (ToadsAndFrogsPosition *)position;

    if (!can_right_move(raw_game, position, e)) {
        return NULL;
    }

    int to = e - 1;
    if (!is_valid_cell(board, to) || !is_empty(pos, to)) {
        to = e - 2;
    }

    ToadsAndFrogsPosition *next = malloc(sizeof *next);
    if (next == NULL) {
        return NULL;
    }

    next->toads_mask = pos->toads_mask;
    next->frogs_mask = pos->frogs_mask;

    next->frogs_mask &= ~bit_at(e);
    next->frogs_mask |= bit_at(to);

    return next;
}

uint64_t hash_raw_game_position(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    ToadsAndFrogsBoard *board = (ToadsAndFrogsBoard *)raw_game;
    ToadsAndFrogsPosition *pos = (ToadsAndFrogsPosition *)position;

    uint64_t h = FNV_OFFSET;

    h ^= board->length;
    h *= FNV_PRIME;

    h ^= (uint64_t)e;
    h *= FNV_PRIME;

    h ^= pos->toads_mask;
    h *= FNV_PRIME;

    h ^= pos->frogs_mask;
    h *= FNV_PRIME;

    return h;
}

int get_independent_components(RawGame_t raw_game, Position_t position, Position_t *sub_masks[]) {
    if (raw_game == NULL || position == NULL || sub_masks == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    if (!has_any_move(raw_game, position)) {
        *sub_masks = NULL;
        return 0;
    }

    ToadsAndFrogsPosition *pos = (ToadsAndFrogsPosition *)position;
    ToadsAndFrogsPosition *copy = malloc(sizeof *copy);
    if (copy == NULL) {
        warning("Got NULL when allocating position.\n");
        *sub_masks = NULL;
        return 0;
    }

    copy->toads_mask = pos->toads_mask;
    copy->frogs_mask = pos->frogs_mask;

    Position_t *da_sub_masks = NULL;
    da_push(da_sub_masks, copy);

    *sub_masks = da_sub_masks;
    return da_len(da_sub_masks);
}
