#ifndef SINGLETONS_H
#define SINGLETONS_H

#include "cgt.h"


void singletons_init(void);

// Gettery pro základní hry
Game* game_zero(void);
Game* game_star(void);
Game* game_one(void);
Game* game_up(void);
Game* game_down(void);

int is_zero(Game *G);
int is_star(Game *G);
int is_one(Game *G);

// násobky šipek
int get_up_arrow_multiple(Game *G);
int get_down_arrow_multiple(Game *G);

void game_print_raw(Game *G);
const char* game_get_string(Game *G);

#endif // SINGLETONS_H
