/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef SHORT_GAME_H
#define SHORT_GAME_H

#include <stdlib.h>

extern int cannon_count;
extern int eq_count;
extern int add_count;
extern int make_count;

/** @brief Interned short-game node representing G = { G^L | G^R }. */
typedef struct Game {
    struct Game **left;  // Dynamic array of pointers to Left's options
    struct Game **right; // Dynamic array of pointers to Right's options
} Game;

/**
 * @brief Allocates the core memoization tables and initializes known games.
 * @param memory_multiplier Fraction of currently available memory to budget.
 */
void short_game_init(float memory_multiplier);

/** @brief Releases all core memoization tables. */
void short_game_free(void);

/** @brief Creates an empty game node { | }. */
Game* game_new();

/** @brief Creates a game by copying the supplied option arrays. */
Game* game_from_games(Game **left, Game **right);

/** @brief Creates a game with at most one option for each player. */
Game *game_from_game(Game *left, Game *right);


/** @brief Tests the Conway order relation G >= H. */
int game_geq(Game *G, Game *H);

/** @brief Tests game equality through the Conway order relation. */
int game_eq(Game *G, Game *H);

/**
 * @brief Recursively converts a game to canonical, interned form.
 *
 * Dominated and reversible options are removed after all descendants have
 * been canonicalized.
 */
Game* game_canonicalize(Game *G);

/** @brief Canonicalizes a node whose descendants are already canonical. */
Game* game_canonicalize_shallow(Game *G);

/** @brief Computes and canonicalizes the disjunctive sum G + H. */
Game* game_add(Game *G, Game *H);


#endif // SHORT_GAME_H
