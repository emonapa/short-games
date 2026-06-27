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

// Representation of a game G = {L | R}
typedef struct Game {
    struct Game **left;  // Dynamic array of pointers to Left's options
    struct Game **right; // Dynamic array of pointers to Right's options
} Game;

void short_game_init(float memory_multiplier);
void short_game_free(void);

// Basic game construction operation.
Game* game_new();
Game* game_from_games(Game **left, Game **right);
Game *game_from_game(Game *left, Game *right);


// Conway game comparison.
int game_geq(Game *G, Game *H); // Returns 1 if G >= H
int game_eq(Game *G, Game *H);  // Returns 1 if G == H

// Converts a game to canonical form by removing dominated and reversible options.
Game* game_canonicalize(Game *G);
Game* game_canonicalize_shallow(Game *G);

// Computes the mathematical sum of games G and H and returns the result in canonical form.
Game* game_add(Game *G, Game *H);


#endif // SHORT_GAME_H
