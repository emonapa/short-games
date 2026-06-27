#include <stdint.h>
#include <stdlib.h>

#include "../../shared/error.h"
#include "../../shared/darray.h"

#include "domineering.h"
#include "raw_game.h"

#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

static uint64_t bit_at(int index) {
    return 1ULL << index;
}

static int board_cells(const DomineeringBoard *board) {
    return board->width * board->height;
}

static int row_of(const DomineeringBoard *board, int cell) {
    return cell / board->width;
}

static int col_of(const DomineeringBoard *board, int cell) {
    return cell % board->width;
}

static int is_cell_free(const DomineeringPosition *pos, int cell) {
    return (pos->occupied_mask & bit_at(cell)) == 0;
}

DomineeringBoard domineering_make_board(uint8_t width, uint8_t height) {
    DomineeringBoard board;

    if (width == 0 || height == 0) {
        error_exit(ERR_OTHER, "Domineering board dimensions must be positive.\n");
    }

    if (width > DOMINEERING_MAX_WIDTH || height > DOMINEERING_MAX_HEIGHT) {
        error_exit(ERR_OTHER, "Domineering board is too large.\n");
    }

    if ((int)width * (int)height > DOMINEERING_MAX_CELLS) {
        error_exit(ERR_OTHER, "Domineering board has too many cells.\n");
    }

    board.width = width;
    board.height = height;

    return board;
}

DomineeringPosition domineering_make_position(uint64_t occupied_mask) {
    DomineeringPosition pos;
    pos.occupied_mask = occupied_mask;
    return pos;
}

int num_moves(RawGame_t raw_game) {
    if (raw_game == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    DomineeringBoard *board = (DomineeringBoard *)raw_game;
    return board_cells(board);
}

int can_left_move(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    DomineeringBoard *board = (DomineeringBoard *)raw_game;
    DomineeringPosition *pos = (DomineeringPosition *)position;

    int cells = board_cells(board);
    if (e < 0 || e >= cells) {
        return 0;
    }

    int row = row_of(board, e);

    if (row + 1 >= board->height) {
        return 0;
    }

    int lower = e + board->width;

    return is_cell_free(pos, e) && is_cell_free(pos, lower);
}

int can_right_move(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    DomineeringBoard *board = (DomineeringBoard *)raw_game;
    DomineeringPosition *pos = (DomineeringPosition *)position;

    int cells = board_cells(board);
    if (e < 0 || e >= cells) {
        return 0;
    }

    int col = col_of(board, e);

    if (col + 1 >= board->width) {
        return 0;
    }

    int right = e + 1;

    return is_cell_free(pos, e) && is_cell_free(pos, right);
}

Position_t do_move_left(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    DomineeringBoard *board = (DomineeringBoard *)raw_game;
    DomineeringPosition *pos = (DomineeringPosition *)position;

    if (!can_left_move(raw_game, position, e)) {
        return NULL;
    }

    DomineeringPosition *next = malloc(sizeof *next);
    if (next == NULL) {
        return NULL;
    }

    int lower = e + board->width;

    next->occupied_mask = pos->occupied_mask;
    next->occupied_mask |= bit_at(e);
    next->occupied_mask |= bit_at(lower);

    return next;
}

Position_t do_move_right(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    DomineeringPosition *pos = (DomineeringPosition *)position;

    if (!can_right_move(raw_game, position, e)) {
        return NULL;
    }

    DomineeringPosition *next = malloc(sizeof *next);
    if (next == NULL) {
        return NULL;
    }

    int right = e + 1;

    next->occupied_mask = pos->occupied_mask;
    next->occupied_mask |= bit_at(e);
    next->occupied_mask |= bit_at(right);

    return next;
}

uint64_t hash_raw_game_position(RawGame_t raw_game, Position_t position, int e) {
    if (raw_game == NULL || position == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    DomineeringBoard *board = (DomineeringBoard *)raw_game;
    DomineeringPosition *pos = (DomineeringPosition *)position;

    uint64_t h = FNV_OFFSET;

    h ^= board->width;
    h *= FNV_PRIME;

    h ^= board->height;
    h *= FNV_PRIME;

    h ^= (uint64_t)e;
    h *= FNV_PRIME;

    h ^= pos->occupied_mask;
    h *= FNV_PRIME;

    return h;
}

static int has_any_move(RawGame_t raw_game, Position_t position) {
    int moves = num_moves(raw_game);

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

/*
 * Proof of concept:
 * Nedelame skutecny rozklad na nezavisle komponenty.
 * Pokud existuje aspon jeden tah, vratime jednu kopii cele pozice.
 * Pokud neexistuje tah, vratime 0 komponent a solve() dostane game_zero().
 */
int get_independent_components(RawGame_t raw_game, Position_t position, Position_t *sub_masks[]) {
    if (raw_game == NULL || position == NULL || sub_masks == NULL) error_exit(ERR_NULL_POINTER, "");

    if (!has_any_move(raw_game, position)) return 0;

    // Pouze se překopírovává příchozí maska
    DomineeringPosition *pos = (DomineeringPosition *)position;
    DomineeringPosition *copy = malloc(sizeof *copy);
    if (copy == NULL) {
        warning("Got NULL when allocating position");
        return 0;
    }
    copy->occupied_mask = pos->occupied_mask;

    Position_t *da_sub_masks = NULL;
    da_push(da_sub_masks, copy);

    *sub_masks = da_sub_masks;
    return da_len(da_sub_masks);
}
