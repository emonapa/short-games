/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#ifndef GAME_FORMAT_H
#define GAME_FORMAT_H

#include "../short_game.h"

enum output_format {
    FORMAT_RAW = 0,
    FORMAT_FORMATED = 1
};

/**
 * @brief Converts a short game to raw or symbolic notation.
 *
 * The returned string is owned by the formatting module and remains valid
 * until the next call to game_get_string().
 *
 * @param game Game to format; NULL is represented by the string "NULL".
 * @param format Requested output notation.
 * @return Module-owned, null-terminated UTF-8 string.
 */
const char *game_get_string(Game *game, enum output_format format);

#endif // GAME_FORMAT_H
