#ifndef CGT_H
#define CGT_H

#include <stdlib.h>

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

// Základní operace
Game* game_make(Game **left, int L_count, Game **right, int R_count);

// Conwayovo porovnávání
int game_geq(Game *G, Game *H); // Vrací 1 pokud G >= H
int game_eq(Game *G, Game *H);  // Vrací 1 pokud G == H

// Kanonizace (odstranění dominovaných a reverzibilních tahů)
Game* game_canonicalize(Game *G);

// Vypocita matematicky soucet dvou her G a H a vrati vysledek v kanonickem tvaru
Game* game_add(Game *G, Game *H);

// Pomocná funkce pro vypsání hry a určení vítěze
void game_print_outcome(Game *G);

void cgt_memo_init(size_t free_ram_bytes);
void cgt_memo_free(void);




#endif // CGT_H
