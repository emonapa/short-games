/*
 * Final bachelors thesis
 * Title cz: Algoritmy strojového hraní Hackenbushe s využitím surreálních čísel
 * Title en: Algorithms for Automated Play of Hackenbush Using Surreal Numbers
 *
 * Faculty of Information Technology Brno University of Technology
 * Author: Václav Matyáš (xmatyav00)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "memory.h"
#include "error.h"
#include "config.h"
#include "stack.h"
#include "darray.h"

#include "game_intern_cache.h"
#include "game_operations_cache.h"
#include "game_canon_cache.h"
#include "position_cache.h"

#include "short_game.h"
#include "singletons.h"
#include "raw_game.h"

int cannon_count = 0;
int eq_count = 0;
int add_count = 0;
int make_count = 0;
int info_count = 100000000;

float g_memory_multiplier = 0.5;

void short_game_init(float memory_multiplier) {
    if (memory_multiplier > 1 || memory_multiplier <= 0)
        error_exit(ERR_OTHER, "%f is invalid fraction argument.\n", memory_multiplier);

    g_memory_multiplier = memory_multiplier;
    size_t free_ram_bytes = get_size_free_memory();

    size_t ram_to_use = free_ram_bytes * memory_multiplier;

    size_t geq_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_GEQ) / sizeof(GeqEntry));
    size_t add_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_ADD) / sizeof(AddEntry));

    size_t canon_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_CANON) / sizeof(CanonEntry));
    size_t intern_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_INTERN) / sizeof(InternEntry));

    size_t pos_size = get_nearest_power_of_2((size_t)(ram_to_use * PCT_POS) / sizeof(HashEntry));

    // inicializace
    game_operations_cache_init(geq_size, add_size);
    game_canon_cache_init(canon_size);
    game_intern_cache_init(intern_size);
    position_cache_init(pos_size);

    singletons_init();
}

void short_game_free(void) {
    game_operations_cache_free_all();
    game_canon_cache_free();
    game_intern_cache_free();
    position_cache_free();
}

/* ------------------------------------------------------------
   Základní tvorba uzlu
   ------------------------------------------------------------ */
Game* game_make(Game **left, int L_count, Game **right, int R_count) {
    Game *g = (Game*)malloc(sizeof(Game));
    if (g == NULL) error_exit(ERR_MALLOC, "");

    g->L_count = L_count;
    g->R_count = R_count;
    //TODO
    make_count++;
    if (make_count % info_count == 0) printf("[INFO] make count %d.   * %d\n", (int)(make_count/info_count), info_count);

    g->left = NULL;
    if (L_count > 0) {
        g->left = (Game**)malloc((size_t)L_count * sizeof(Game*));
        if (g->left == NULL) error_exit(ERR_MALLOC, "");

        for (int i = 0; i < L_count; i++) g->left[i] = left[i];
    }

    g->right = NULL;
    if (R_count > 0) {
        g->right = (Game**)malloc((size_t)R_count * sizeof(Game*));
        if (g->right == NULL) error_exit(ERR_MALLOC, "");

        for (int i = 0; i < R_count; i++) g->right[i] = right[i];
    }

    return g;
}

//#define GEQ_RECURSIVE

#ifndef GEQ_RECURSIVE
// Frame pro zasobnik
typedef struct {
    Game *G;
    Game *H;
    int stage; // 0 = init, 1 = cyklus G->right, 2 = cyklus H->left
    int i;     // iterator pro cykly
} GeqFrame;


int game_geq(Game *G_root, Game *H_root) {
    if (G_root == NULL || H_root == NULL) error_exit(ERR_NULL_POINTER, "");

    uint8_t first_memo;
    if (game_geq_cache_get(G_root, H_root, &first_memo)) return first_memo;

    TStack stack;
    stack_init(&stack, sizeof(GeqFrame));

    GeqFrame root = { G_root, H_root, 0, 0 };
    Push(&stack, &root);

    // last_ret funguje jako predavani navratove hodnoty od potomka rodici.
    // -1 znamena "zadna navratova hodnota neceka na zpracovani".
    int last_ret = -1;
    size_t size_stack = 0;
    while (!IsEmpty(&stack)) {
        GeqFrame *f = (GeqFrame *)Top(&stack);
        // STAGE 0: Inicializace, kontrola base-cases a memoizace
        if (f->stage == 0) {
            if (f->G == f->H) {
                last_ret = 1;
                Pop(&stack);
                continue;
            }

            uint8_t memo;
            if (game_geq_cache_get(f->G, f->H, &memo)) {
                last_ret = (int)memo;
                Pop(&stack);
                continue;
            }

            //TODO
            eq_count++;
            if (eq_count % info_count == 0)
                printf("[INFO] eq count %d.   *%d\n", (int)(eq_count/info_count), info_count);

            if (stack.size > size_stack) size_stack = stack.size;

            f->stage = 1;
            f->i = 0;
            continue;
        }

        // STAGE 1: pruchody f->G->right[i]
        if (f->stage == 1) {
            // Pokud jsme se prave vratili z potomka, zkontrolujeme jeho vysledek
            if (last_ret != -1) {
                if (last_ret == 1) { // Potomek (H >= G->right) vratil true
                    game_geq_cache_put(f->G, f->H, 0);
                    last_ret = 0; // Vracime false pro rodice
                    Pop(&stack);
                    continue;
                }
                last_ret = -1; // Potomek vratil false, jdeme na dalsi iteraci
            }

            // Pokud mame jeste co prochazet v G->right
            if (f->i < f->G->R_count) {
                Game *next_G = f->H;
                Game *next_H = f->G->right[f->i];

                f->i++;

                GeqFrame child = { next_G, next_H, 0, 0 };
                Push(&stack, &child);
                continue;
            }

            // Pokud jsme prosli cele G->right a nezastavili se, prepneme na STAGE 2
            f->stage = 2;
            f->i = 0;
            continue;
        }

        // STAGE 2: pruchody f->H->left[i]
        if (f->stage == 2) {
            // Kontrola vysledku z predchoziho potomka
            if (last_ret != -1) {
                if (last_ret == 1) { // Potomek (H->left >= G) vratil true
                    game_geq_cache_put(f->G, f->H, 0);
                    last_ret = 0; // Vracime false pro rodice
                    Pop(&stack);
                    continue;
                }
                last_ret = -1;
            }

            // Pokud mame jeste co prochazet v H->left
            if (f->i < f->H->L_count) {
                Game *next_G = f->H->left[f->i];
                Game *next_H = f->G;

                f->i++;

                GeqFrame child = { next_G, next_H, 0, 0 };
                Push(&stack, &child);
                continue;
            }

            // Vsechny testy prosly => G >= H
            game_geq_cache_put(f->G, f->H, 1);
            last_ret = 1;
            Pop(&stack);
            continue;
        }
    }

    stack_dtor(&stack);
    return last_ret;
}
#else
int game_geq(Game *G, Game *H) {
    if (G == NULL || H == NULL) error_exit(ERR_NULL_POINTER, "");

    if (G == H) return 1;

    uint8_t memo;
    if (game_geq_cache_get(G, H, &memo)) return (int)memo;

    // První část definice:
    // Pro každý pravý tah G^R musí platit:
    //     !(H >= G^R)
    //
    // Pokud najdeme pravý tah G^R takový, že H >= G^R,
    // potom G >= H neplatí.
    for (int i = 0; i < G->R_count; i++) {
        Game *GR = G->right[i];

        if (game_geq(H, GR)) {
            game_geq_cache_put(G, H, 0);
            return 0;
        }
    }

    // Druhá část definice:
    // Pro každý levý tah H^L musí platit:
    //     !(H^L >= G)
    //
    // Pokud najdeme levý tah H^L takový, že H^L >= G,
    // potom G >= H neplatí.
    for (int i = 0; i < H->L_count; i++) {
        Game *HL = H->left[i];

        if (game_geq(HL, G)) {
            game_geq_cache_put(G, H, 0);
            return 0;
        }
    }

    // Pokud neexistuje žádný problémový pravý tah z G
    // ani žádný problémový levý tah z H, pak G >= H.
    game_geq_cache_put(G, H, 1);
    return 1;
}
#endif

int game_eq(Game *G, Game *H) {
    return game_geq(G, H) && game_geq(H, G);
}

static void replace_left_option(Game *G, int index, Game *GLR) {
    if (G == NULL || GLR == NULL) error_exit(ERR_NULL_POINTER, "");

    int old_count = G->L_count;
    int replace_count = GLR->L_count;
    int new_count = old_count - 1 + replace_count;

    Game **new_arr = NULL;
    if (new_count > 0) {
        new_arr = (Game**)malloc(new_count * sizeof(Game*));
        if (new_arr == NULL) error_exit(ERR_MALLOC, "");

        int dst = 0;
        // 1. Zkopírujeme staré tahy před indexem
        for (int i = 0; i < index; i++) new_arr[dst++] = G->left[i];
        // 2. Vložíme vnuky (tahy z potrestané pozice)
        for (int i = 0; i < replace_count; i++) new_arr[dst++] = GLR->left[i];
        // 3. Zkopirujeme staré tahy za indexem
        for (int i = index + 1; i < old_count; i++) new_arr[dst++] = G->left[i];
    }

    if (G->left) free(G->left); // Uvolníme původní pole
    G->left = new_arr;
    G->L_count = new_count;
}

static void replace_right_option(Game *G, int index, Game *GRL) {
    if (G == NULL || GRL == NULL) error_exit(ERR_NULL_POINTER, "");

    int old_count = G->R_count;
    int replace_count = GRL->R_count;
    int new_count = old_count - 1 + replace_count;

    Game **new_arr = NULL;
    if (new_count > 0) {
        new_arr = (Game**)malloc(new_count * sizeof(Game*));
        if(new_arr == NULL) error_exit(ERR_MALLOC, "");

        int dst = 0;
        for (int i = 0; i < index; i++) new_arr[dst++] = G->right[i];
        for (int i = 0; i < replace_count; i++) new_arr[dst++] = GRL->right[i];
        for (int i = index + 1; i < old_count; i++) new_arr[dst++] = G->right[i];
    }

    if (G->right) free(G->right);
    G->right = new_arr;
    G->R_count = new_count;
}

// -----------------------------------------------------------------
// HLAVNÍ FUNKCE KANONIZACE
// -----------------------------------------------------------------
// 1. Odstranit leve reverzibilni tahy
// 2. Odstranit prave reverzibilni tahy
// 3. Odstranit leve dominovane tahy
// 4. Odstranit prave dominovane tahy
// 5. Najit sebe v intern cache
Game* game_canonicalize(Game *G) {
    if (G == NULL) {
        error_exit(ERR_NULL_POINTER, "");
    }

    if (G == game_zero() || G == game_star()) return G;

    Game *cached = NULL;
    if (game_canon_cache_get(G, &cached)) return cached;

    cannon_count++;

    // 1) Kanonizuj potomky (zajistí, že GLR už jsou čisté intern pointery)
    // Ok tohle teoreticky nemusime delat, protoze v klasickem solvu mame zaruceno,
    //  ze kazda podhra je zkanonizovana, problem je napr v kalkulacce kde to
    //  zaruceno neni.
    for (int i = 0; i < G->L_count; i++) G->left[i] = game_canonicalize(G->left[i]);
    for (int i = 0; i < G->R_count; i++) G->right[i] = game_canonicalize(G->right[i]);

    int changed = 1;
    while (changed) {
        changed = 0;

        // 2) Reverzibilní levé tahy
        for (int i = 0; i < G->L_count; i++) {
            Game *GL = G->left[i];
            int reversed = 0;

            for (int j = 0; j < GL->R_count; j++) {
                Game *GLR = GL->right[j];
                // Test: Pokud má cervený odpověď, která ho uspokojí (GLR <= G)
                if (game_geq(G, GLR)) {
                    replace_left_option(G, i, GLR); // Vyhodí past a vloží LRL
                    reversed = 1;
                    break;
                }
            }
            if (reversed) {
                changed = 1;
                break; // Pole tahů se změnilo, jdeme raději odznova
            }
        }
        if (changed) continue;

        // 3) Reverzibilní pravé tahy
        for (int i = 0; i < G->R_count; i++) {
            Game *GR = G->right[i];
            int reversed = 0;

            for (int j = 0; j < GR->L_count; j++) {
                Game *GRL = GR->left[j];
                // Test: Pokud má Modrý odpověď, která ho uspokojí (GRL >= G)
                if (game_geq(GRL, G)) {
                    replace_right_option(G, i, GRL); // Vyhodí past a vloží RLR
                    reversed = 1;
                    break;
                }
            }
            if (reversed) {
                changed = 1;
                break;
            }
        }
        if (changed) continue;

        // 4) Dominované levé tahy
        int keep_L = 0;
        for (int i = 0; i < G->L_count; i++) {
            Game *cand = G->left[i];
            int dominated = 0;

            for (int j = 0; j < keep_L; j++) {
                Game *kept = G->left[j];
                if (kept == cand || game_geq(kept, cand)) {
                    dominated = 1;
                    break;
                }
                if (game_geq(cand, kept)) {
                    G->left[j] = G->left[--keep_L];
                    j--;
                }
            }
            if (!dominated) G->left[keep_L++] = cand;
        }
        if (keep_L != G->L_count) {
            G->L_count = keep_L;
            changed = 1;
        }
        if (changed) continue;

        // 5) Dominované pravé tahy
        int keep_R = 0;
        for (int i = 0; i < G->R_count; i++) {
            Game *cand = G->right[i];
            int dominated = 0;

            for (int j = 0; j < keep_R; j++) {
                Game *kept = G->right[j];
                if (kept == cand || game_geq(cand, kept)) {
                    dominated = 1;
                    break;
                }
                if (game_geq(kept, cand)) {
                    G->right[j] = G->right[--keep_R];
                    j--;
                }
            }
            if (!dominated) G->right[keep_R++] = cand;
        }
        if (keep_R != G->R_count) {
            G->R_count = keep_R;
            changed = 1;
        }
    }

    // 6) Intern kanonický uzel
    game_intern_cache_prepare(G);
    Game *I = game_intern_cache_get(G);

    game_canon_cache_put(G, I);
    return I;
}


/* ------------------------------------------------------------
   Součet her s memoizací
   ------------------------------------------------------------ */
Game* game_add(Game *G, Game *H) {
    if (G == NULL && H == NULL) error_exit(ERR_NULL_POINTER, "Both games to add are NULL.\n");
    if (!G) return H;
    if (!H) return G;

    if (G->L_count == 0 && G->R_count == 0) return H;
    if (H->L_count == 0 && H->R_count == 0) return G;

    // komutativni normalizace klice
    if ((uintptr_t)G > (uintptr_t)H) {
        Game *tmp = G; G = H; H = tmp;
    }

    Game *memo = NULL;
    if (game_add_cache_get(G, H, &memo)) return memo;

    //TODO
    add_count++;
    if (add_count % info_count == 0) printf("[INFO] add count %d.   *%d\n", (int)(add_count/info_count), info_count);

    int new_l_count = G->L_count + H->L_count;
    int new_r_count = G->R_count + H->R_count;

    Game **left_opts = NULL;
    Game **right_opts = NULL;
    if (new_l_count > 0) {
        left_opts = (Game**)malloc((size_t)new_l_count * sizeof(Game*));
        if (left_opts == NULL) error_exit(ERR_MALLOC, "");
    }
    if (new_r_count > 0) {
        right_opts = (Game**)malloc((size_t)new_r_count * sizeof(Game*));
        if (right_opts == NULL) error_exit(ERR_MALLOC, "");
    }

    int idx = 0;
    for (int i = 0; i < G->L_count; i++) left_opts[idx++] = game_add(G->left[i], H);
    for (int i = 0; i < H->L_count; i++) left_opts[idx++] = game_add(G, H->left[i]);

    idx = 0;
    for (int i = 0; i < G->R_count; i++) right_opts[idx++] = game_add(G->right[i], H);
    for (int i = 0; i < H->R_count; i++) right_opts[idx++] = game_add(G, H->right[i]);

    Game *sum = game_make(left_opts, new_l_count, right_opts, new_r_count);
    sum = game_canonicalize(sum);
    if (sum == NULL) warning("Got NULL from canonization.\n");

    if (left_opts) free(left_opts);
    if (right_opts) free(right_opts);

    game_add_cache_put(G, H, sum);
    return sum;
}


Game* solve_component(RawGame raw_game, Position_t position) {
    if (raw_game == NULL || position == NULL) error_exit(ERR_NULL_POINTER, "");

    // Memoizace
    Game *memo = NULL;
    if (position_cache_get(raw_game, position, &memo))
        return memo;

    Game **left_opts = NULL;
    Game **right_opts = NULL;
    int l_count = 0, r_count = 0;

    // Rekurzivní průchod všemi tahy
    for (int e = 0; e < num_moves(raw_game); ++e) {
        Game *child = NULL;
        if (can_left_move(raw_game, position, e)) {
            Position_t child_position = do_move_left(raw_game, position, e);
            if (child_position == NULL) error_exit(ERR_MALLOC, "");

            child = solve_component(raw_game, child_position);
            da_push(left_opts, child);
        }
        if (can_right_move(raw_game, position, e)) {
            Position_t child_position = do_move_right(raw_game, position, e);
            if (child_position == NULL) error_exit(ERR_MALLOC, "");

            child = solve_component(raw_game, child_position);
            da_push(right_opts, child);
        }
    }

    Game *G = game_canonicalize(game_make(left_opts, (int)da_len(left_opts),
                                          right_opts, (int)da_len(right_opts)));

    da_free(left_opts);
    da_free(right_opts);
    position_cache_insert(raw_game, position, G);
    return G;
}

static void print_stats() {
    printf("[CACHE] canon_count     = %ld\n", canon_items_count);
    printf("[CACHE] intern_count    = %ld\n", intern_items_count);
    printf("[CACHE] add_count       = %ld\n", add_items_count);
    printf("[CACHE] geq_count       = %ld\n", geq_items_count);
    printf("[CACHE] pos_items_count = %ld\n", pos_items_count);
    printf("[META] stack_count      = %ld\n", stack_items_count);
    printf("[META] make_count       = %d\n", make_count);
}

//#define PRINT_RESULT

Game* solve(void *raw_game, void *position) {
    Position_t *sub_games = NULL;
    int count = get_independent_components(raw_game, position, &sub_games);

    Game *total_sum = game_zero();


#ifdef PRINT_RESULT
    printf("======================END RESULT==========================\n");
#endif
    // Spocitame dalsi a pricteme k mezivysledku
    for (int i = 0; i < count; i++) {
        Game *sub_game = solve_component(raw_game, sub_games[i]);
        total_sum = game_add(total_sum, sub_game);
#ifdef PRINT_RESULT
       printf("-------------[%d] Průchod-------------\n", i);
       print_stats();
#endif
        // Grafy se budou pravdepodobne lisit, takze cache resetneme
        // Ale hinty a edu mode budou EXTREMNE pomale, achjo...
        //solver_free();
        //solver_initialize(g_memory_multiplier);
    }
#ifdef PRINT_RESULT
    printf("=========================================================\n");
#endif

    game_canonicalize(total_sum);

#ifdef PRINT_RESULT
    const char *game_string = game_get_string(total_sum, FORMAT_FORMATED);
    printf("Result: %s", game_string);
#endif

    for (int i = 0; i < count; i++) {
        free(sub_games[i]);
    }
    //V tomto případě možné uvolnit takovýmto způsobem, ale ztrácí to obecnost.
    //da_free(sub_games);

    return total_sum;
}
