/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef GAME_STRING_H
#define GAME_STRING_H

#include "../short_game.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parses and evaluates a short-game expression.
 * @param text Null-terminated UTF-8 source text.
 * @return Canonical game, or NULL when any language phase fails.
 */
Game *game_from_string(const char *text);

/**
 * @brief Returns the diagnostic from the most recent game_from_string() call.
 * @return Module-owned string; empty after a successful call.
 */
const char *game_string_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // GAME_STRING_H
