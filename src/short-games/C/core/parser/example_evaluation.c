/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hotpotche s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hotpotch Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include <stdio.h>

#include "game_format.h"
#include "game_string.h"
#include "../singletons.h"

int main(void) {
    short_game_init(0.25f);

    const char *examples[] = {
        "{ | }",
        "{0 | } + *",
        "*2 + *2",
        "2 * {0 | 1}",
        "{0 | 1} / 2",
        "cool({0 | *})",
        "projection({0 | *})"
    };

    for (size_t i = 0; i < sizeof(examples) / sizeof(examples[0]); i++) {
        Game *result = game_from_string(examples[i]);
        if (result == NULL) {
            printf("%-24s -> error: %s\n",
                   examples[i], game_string_last_error());
            continue;
        }

        printf("%-24s -> %s\n",
               examples[i], game_get_string(result, FORMAT_FORMATED));
    }

    short_game_free();
    return 0;
}
