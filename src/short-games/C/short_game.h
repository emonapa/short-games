/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hackenbushe s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hackenbush Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef SHORT_GAME_H
#define SHORT_GAME_H

#include <stdlib.h>
#include "raw_game.h"

extern int cannon_count;
extern int eq_count;
extern int add_count;
extern int make_count;

// Representation of a game G = {L | R}
typedef struct Game {
    int L_count;
    int R_count;
    struct Game **left;  // Array of pointers to Left's options
    struct Game **right; // Array of pointers to Right's options
} Game;

void short_game_init(float memory_multiplier);
void short_game_free(void);

// Basic game construction operation.
Game* game_make(Game **left, int L_count, Game **right, int R_count);

// Conway game comparison.
int game_geq(Game *G, Game *H); // Returns 1 if G >= H
int game_eq(Game *G, Game *H);  // Returns 1 if G == H

// Converts a game to canonical form by removing dominated and reversible options.
Game* game_canonicalize(Game *G);

// Computes the mathematical sum of games G and H and returns the result in canonical form.
Game* game_add(Game *G, Game *H);

// Solves a single connected component of the raw Hackenbush position.
Game* solve_component(RawGame_t raw_game, Position_t position);

// Splits the position into independent components and sums their game values.
Game* solve(RawGame_t raw_game, Position_t position);

#endif // SHORT_GAME_H
