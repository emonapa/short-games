#ifndef SINGLETONS_H
#define SINGLETONS_H

#include "short_game.h"

enum output_format {
    FORMAT_RAW       = 0,   // only substitutes 0 = {|}, everything else as {L|R}
    FORMAT_FORMATED  = 1    // full symbolic output: ↑, *, 1/2, ...
};

void   singletons_init(void);

// -- Basic singletons -------------------------------------------------------
Game*  game_zero(void);
Game*  game_star(void);
Game*  game_one(void);
Game*  game_up(void);
Game*  game_down(void);

int    is_zero(Game *G);
int    is_star(Game *G);
int    is_one(Game *G);

// -- Output -----------------------------------------------------------------
const char* game_get_string(Game *G, enum output_format format);

// -- Construction helpers (used by calculator / Python) ---------------------
Game*  make_int(int n);
Game*  make_dyadic(int p, int q);   // q must be a power of 2
Game*  make_nimber(int n);
Game*  make_up_multiple(int n, int with_star);
Game*  make_down_multiple(int n, int with_star);

int is_number(Game *G);
int is_dyadic_plus_star(Game *G, double *out_dyadic_val);
int get_dyadic_value(Game *G, double *out_val);

Game* game_negate(Game *G);
Game* cool_with_star(Game *G);
Game* star_projection(Game *H);

#endif // SINGLETONS_H
