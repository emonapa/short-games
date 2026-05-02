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

// Reprezentace hry G = {L | R}
typedef struct Game {
    int L_count;
    int R_count;
    struct Game **left;  // Pole pointerů na levé možnosti
    struct Game **right; // Pole pointerů na pravé možnosti
} Game;

void short_game_init(float memory_multiplier);
void short_game_free(void);

// Základní operace
Game* game_make(Game **left, int L_count, Game **right, int R_count);

// Conwayovo porovnávání
int game_geq(Game *G, Game *H); // Vrací 1 pokud G >= H
int game_eq(Game *G, Game *H);  // Vrací 1 pokud G == H

// Kanonizace (odstranění dominovaných a reverzibilních tahů)
Game* game_canonicalize(Game *G);

// Vypocita matematicky soucet dvou her G a H a vrati vysledek v kanonickem tvaru
Game* game_add(Game *G, Game *H);

Game* solve_component(RawGame raw_game, Position_t position);
// Rozdeli pozici na nezavisle komponenty a secte je
Game* solve(RawGame raw_game, Position_t position);

#endif // SHORT_GAME_H
