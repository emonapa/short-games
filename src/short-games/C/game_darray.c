#include "game_darray.h"

size_t game_len(Game ***game) {
    return da_len(*game);
}

size_t game_cap(Game ***game) {
    return da_cap(*game);
}

void game_free(Game ***game) {
    da_free(*game);
}

void game_reserve(Game ***game, size_t expected_cap) {
    da_reserve(*game, expected_cap);
}

void game_push(Game ***game, Game *value) {
    da_push(*game, value);
}

void game_append(Game ***game, Game *value) {
    da_append(*game, value);
}

void game_append_many(Game ***game, Game **other) {
    da_append_many(*game, other);
}

void game_resize(Game ***game, size_t new_len) {
    da_resize(*game, new_len);
}

Game *game_pop(Game ***game) {
    return da_pop(*game);
}

Game *game_first(Game ***game) {
    return da_first(*game);
}

Game *game_last(Game ***game) {
    return da_last(*game);
}

void game_remove_unordered(Game ***game, size_t index) {
    da_remove_unordered(*game, index);
}
