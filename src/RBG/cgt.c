#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "game_intern_cache.h"
#include "game_operations_cache.h"
#include "game_canon_cache.h"

#include "stack.h"
#include "memory.h"
#include "config.h"
#include "singletons.h"

int cannon_count = 0;
int eq_count = 0;
int add_count = 0;
int make_count = 0;
int info_count = 1000000;


void cgt_memo_init(size_t free_ram_bytes) {
    size_t ram_to_use = free_ram_bytes * HOW_MUCH_MEM;

    // rozdeleni procent
    double pct_geq    = 0.88;
    double pct_canon  = 0.03;
    double pct_intern = 0.36;
    double pct_add    = 0.02;

    size_t geq_size = get_nearest_power_of_2((size_t)(ram_to_use * pct_geq) / sizeof(GeqEntry));
    size_t add_size = get_nearest_power_of_2((size_t)(ram_to_use * pct_add) / sizeof(AddEntry));

    size_t canon_size = get_nearest_power_of_2((size_t)(ram_to_use * pct_canon) / sizeof(CanonEntry));
    size_t intern_size = get_nearest_power_of_2((size_t)(ram_to_use * pct_intern) / sizeof(InternEntry));

    printf("Sizeof GeqEntry = %ld\n", sizeof(GeqEntry));
    printf("Sizeof AddEntry = %ld\n", sizeof(AddEntry));
    printf("Sizeof CanonEntry = %ld\n", sizeof(CanonEntry));
    printf("Sizeof InternEntry = %ld\n", sizeof(InternEntry));

    printf("geq_size = %gMB\n", (ram_to_use * pct_geq)/(1024*1024));
    printf("add_size = %gMB\n", (ram_to_use * pct_add)/(1024*1024));
    printf("canon_size = %gMB\n", (ram_to_use * pct_canon)/(1024*1024));
    printf("intern_size = %gMB\n", (ram_to_use * pct_intern)/(1024*1024));

    // inicializace
    game_operations_cache_init(geq_size, add_size);
    game_canon_cache_init(canon_size);
    game_intern_cache_init(intern_size);

    singletons_init();
}

void cgt_memo_free(void) {
    game_operations_cache_free_all();
    game_canon_cache_free();
    game_intern_cache_free();
}

/* ------------------------------------------------------------
   Základní tvorba uzlu
   ------------------------------------------------------------ */
Game* game_make(Game **left, int L_count, Game **right, int R_count) {
    Game *g = (Game*)malloc(sizeof(Game));
    g->L_count = L_count;
    g->R_count = R_count;
    //TODO
    make_count++;
    if (make_count % info_count == 0) printf("[INFO] make count %d.   * %d\n", (int)(make_count/info_count), info_count);

    g->left = NULL;
    if (L_count > 0) {
        g->left = (Game**)malloc((size_t)L_count * sizeof(Game*));
        for (int i = 0; i < L_count; i++) g->left[i] = left[i];
    }

    g->right = NULL;
    if (R_count > 0) {
        g->right = (Game**)malloc((size_t)R_count * sizeof(Game*));
        for (int i = 0; i < R_count; i++) g->right[i] = right[i];
    }

    return g;
}








// Frame pro zasobnik
typedef struct {
    Game *G;
    Game *H;
    int stage; // 0 = init, 1 = cyklus G->right, 2 = cyklus H->left
    int i;     // iterator pro cykly
} GeqFrame;

int game_geq(Game *G_root, Game *H_root) {
    TStack stack;
    stack_init(&stack, sizeof(GeqFrame));

    GeqFrame root = { G_root, H_root, 0, 0 };
    Push(&stack, &root);

    // last_ret funguje jako predavani navratove hodnoty od potomka rodici.
    // -1 znamena "zadna navratova hodnota neceka na zpracovani".
    int last_ret = -1;

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
            if (eq_count % info_count == 0) {
                printf("[INFO] eq count %d.   *%d\n", (int)(eq_count/info_count), info_count);
            }
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

int game_eq(Game *G, Game *H) {
    return game_geq(G, H) && game_geq(H, G);
}



static void replace_left_option(Game *G, int index, Game *GLR) {
    int old_count = G->L_count;
    int add_count = GLR->L_count;
    int new_count = old_count - 1 + add_count;

    Game **new_arr = NULL;
    if (new_count > 0) {
        new_arr = (Game**)malloc(new_count * sizeof(Game*));
        int dst = 0;
        // 1. Zkopírujeme staré tahy před indexem
        for (int i = 0; i < index; i++) new_arr[dst++] = G->left[i];
        // 2. Vložíme vnuky (tahy z potrestané pozice)
        for (int i = 0; i < add_count; i++) new_arr[dst++] = GLR->left[i];
        // 3. Zkopirujeme staré tahy za indexem
        for (int i = index + 1; i < old_count; i++) new_arr[dst++] = G->left[i];
    }

    if (G->left) free(G->left); // Uvolníme původní pole
    G->left = new_arr;
    G->L_count = new_count;
}

static void replace_right_option(Game *G, int index, Game *GRL) {
    int old_count = G->R_count;
    int add_count = GRL->R_count;
    int new_count = old_count - 1 + add_count;

    Game **new_arr = NULL;
    if (new_count > 0) {
        new_arr = (Game**)malloc(new_count * sizeof(Game*));
        int dst = 0;
        for (int i = 0; i < index; i++) new_arr[dst++] = G->right[i];
        for (int i = 0; i < add_count; i++) new_arr[dst++] = GRL->right[i];
        for (int i = index + 1; i < old_count; i++) new_arr[dst++] = G->right[i];
    }

    if (G->right) free(G->right);
    G->right = new_arr;
    G->R_count = new_count;
}

// -----------------------------------------------------------------
// HLAVNÍ FUNKCE KANONIZACE
// -----------------------------------------------------------------

Game* game_canonicalize(Game *G) {
    if (!G) return NULL;
    if (G == game_zero() || G == game_star()) return G;

    Game *cached = NULL;
    if (game_canon_cache_get(G, &cached)) return cached;

    cannon_count++;

    // 1) Kanonizuj potomky (zajistí, že GLR už jsou čisté intern pointery)
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

    Game **left_opts = new_l_count > 0 ? (Game**)malloc((size_t)new_l_count * sizeof(Game*)) : NULL;
    Game **right_opts = new_r_count > 0 ? (Game**)malloc((size_t)new_r_count * sizeof(Game*)) : NULL;

    int idx = 0;
    for (int i = 0; i < G->L_count; i++) left_opts[idx++] = game_add(G->left[i], H);
    for (int i = 0; i < H->L_count; i++) left_opts[idx++] = game_add(G, H->left[i]);

    idx = 0;
    for (int i = 0; i < G->R_count; i++) right_opts[idx++] = game_add(G->right[i], H);
    for (int i = 0; i < H->R_count; i++) right_opts[idx++] = game_add(G, H->right[i]);

    Game *sum = game_make(left_opts, new_l_count, right_opts, new_r_count);
    sum = game_canonicalize(sum);

    if (left_opts) free(left_opts);
    if (right_opts) free(right_opts);

    game_add_cache_put(G, H, sum);
    return sum;
}

/* ------------------------------------------------------------
   Outcome
   ------------------------------------------------------------ */
void game_print_outcome(Game *G) {
    Game *zero = game_zero();
    int g_geq_0 = game_geq(G, zero);
    int zero_geq_g = game_geq(zero, G);

    if (g_geq_0 && !zero_geq_g) {
        printf("Vysledek: G > 0 (Vyhrava Modry / Left)\n");
    } else if (!g_geq_0 && zero_geq_g) {
        printf("Vysledek: G < 0 (Vyhrava Cerveny / Right)\n");
    } else if (g_geq_0 && zero_geq_g) {
        printf("Vysledek: G = 0 (Vyhrava Druhy na tahu / Second)\n");
    } else {
        printf("Vysledek: G || 0 (Fuzzy pozice, vyhrava Prvni na tahu / First)\n");
    }
}
