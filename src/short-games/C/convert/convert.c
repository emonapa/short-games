#include <stdint.h>
#include <stdio.h>

#include "../config.h"
#include "../shared/error.h"
#include "../shared/memory.h"

#include "../core/singletons.h"
#include "../core/game_darray.h"
#include "../core/short_game.h"

#include "position_cache.h"
#include "convert_interface/raw_game.h"

void convert_init(float memory_multiplier) {
    if (memory_multiplier > 1 || memory_multiplier <= 0)
        error_exit(ERR_OTHER, "%f is invalid fraction argument.\n", memory_multiplier);

    //g_memory_multiplier = memory_multiplier;
    size_t free_ram_bytes = get_size_free_memory();
    size_t ram_to_use = free_ram_bytes * memory_multiplier;

    size_t pos_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_POS) / sizeof(HashEntry));

    // Initialize all caches according to the configured memory budget.
    position_cache_init(pos_size);

}

void convert_free(void) {
    position_cache_free();
}


Game* solve_component(RawGame_t raw_game, Position_t position) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");

    // Memoization for already solved raw positions.
    Game *memo = NULL;
    if (position_cache_get(raw_game, position, &memo))
        return memo;

    Game **left_opts = NULL;
    Game **right_opts = NULL;

    // Recursively evaluate all legal moves from this position.
    for (int e = 0; e < num_moves(raw_game); ++e) {
        Game *child = NULL;
        if (can_left_move(raw_game, position, e)) {
            Position_t child_position = do_move_left(raw_game, position, e);
            if (child_position == NULL) error_exit(ERR_MALLOC, "");

            child = solve_component(raw_game, child_position);
            game_push(&left_opts, child);
            free(child_position);
        }
        if (can_right_move(raw_game, position, e)) {
            Position_t child_position = do_move_right(raw_game, position, e);
            if (child_position == NULL) error_exit(ERR_MALLOC, "");

            child = solve_component(raw_game, child_position);
            game_push(&right_opts, child);
            free(child_position);
        }
    }

    Game *G = game_canonicalize_shallow(game_from_games(left_opts, right_opts));

    game_free(&left_opts);
    game_free(&right_opts);
    position_cache_insert(raw_game, position, G);
    return G;
}

static void print_stats() {
    printf("[CACHE] canon_count     = %ld\n", canon_items_count);
    printf("[CACHE] intern_count    = %ld\n", intern_items_count);
    printf("[CACHE] add_count       = %ld\n", add_items_count);
    printf("[CACHE] geq_count       = %ld\n", geq_items_count);
    printf("[CACHE] pos_items_count = %ld\n", pos_items_count);
    printf("[META] make_count       = %d\n", make_count);
}

//#define PRINT_RESULT

Game* solve(void *raw_game, void *position) {
    Position_t *sub_games = NULL;
    int count = get_independent_components(raw_game, position, &sub_games);

    Game *total_sum = game_zero();


#ifdef PRINT_RESULT
    printf("======================END RESULT==========================\n");
#endif
    // Solve each independent component and add it to the accumulated game value.
    for (int i = 0; i < count; i++) {
        Game *sub_game = solve_component(raw_game, sub_games[i]);
        total_sum = game_add(total_sum, sub_game);
#ifdef PRINT_RESULT
       printf("-------------[%d] Průchod-------------\n", i);
       print_stats();
#endif
       // Components are usually different, so the cache could be reset here,
       // but hints and educational mode would become extremely slow if this were enabled.
       //solver_free();
       //solver_initialize(g_memory_multiplier);
    }
#ifdef PRINT_RESULT
    printf("=========================================================\n");
#endif

    game_canonicalize(total_sum);

#ifdef PRINT_RESULT
    const char *game_string = game_get_string(total_sum, FORMAT_FORMATED);
    printf("Result: %s", game_string);
#endif

    for (int i = 0; i < count; i++) free(sub_games[i]);
    // This freeing is possible here, but it makes the code less general, so just leak it!
    //game_free(sub_games);

    return total_sum;
}
