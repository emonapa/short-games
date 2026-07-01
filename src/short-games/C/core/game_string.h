/*
 * Parser for textual short-game notation.
 */

#ifndef GAME_STRING_H
#define GAME_STRING_H

#include "short_game.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parses a textual game and returns its canonical Game*.
 *
 * Supported forms:
 *   { L1, L2 | R1, R2 }
 *   *
 *   *n
 *   n
 *   p/q
 *   ↑, ↓, ^, v
 *   n↑, n↓, n^, nv
 *   ↑*, ↓*, ^*, v*
 *   ↑ + *, ↓ + *, ^ + *, v + *
 *
 * On invalid input returns NULL. Use game_string_last_error() for details.
 */
Game *game_from_string(const char *text);

/* Last parser error. The returned pointer is owned by the parser module. */
const char *game_string_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // GAME_STRING_H
