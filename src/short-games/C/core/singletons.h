/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef SINGLETONS_H
#define SINGLETONS_H

#include "short_game.h"

/** @brief Initializes the interned constants used by the core operations. */
void singletons_init(void);

Game *game_zero(void);
Game *game_star(void);
Game *game_one(void);
Game *game_up(void);
Game *game_down(void);
Game *game_up_star(void);
Game *game_down_star(void);

int is_zero(Game *game);
int is_star(Game *game);
int is_one(Game *game);

/** @brief Builds the canonical short game representing an integer. */
Game *make_int(int value);

/**
 * @brief Builds the canonical short game for a dyadic rational.
 * @param numerator Signed numerator.
 * @param denominator Positive power-of-two denominator.
 * @return Canonical game, or NULL when the denominator is invalid.
 */
Game *make_dyadic(int numerator, int denominator);

/** @brief Builds the nimber *n; returns NULL for a negative value. */
Game *make_nimber(int value);

Game *make_up_multiple(int count, int with_star);
Game *make_down_multiple(int count, int with_star);

/** @brief Tests whether a canonical game is a surreal number. */
int is_number(Game *game);
int is_dyadic_plus_star(Game *game, double *out_value);

/** @brief Extracts the floating-point value of a canonical dyadic game. */
int get_dyadic_value(Game *game, double *out_value);

/** @brief Returns the additive inverse -G in canonical form. */
Game *game_negate(Game *game);

/**
 * @brief Computes the Conway product of two short games.
 *
 * Products encountered during one call are memoized by operand pointers.
 * The returned game is canonical.
 *
 * @return Product game, or NULL if either operand is NULL.
 */
Game *game_multiply(Game *left, Game *right);

/** @brief Applies cooling by star to a short game. */
Game *cool_with_star(Game *game);

/** @brief Computes the star projection of a short game. */
Game *star_projection(Game *game);

#endif // SINGLETONS_H
