#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "singletons.h"
#include "cgt.h"

static Game *val_zero = NULL;
static Game *val_star = NULL;
static Game *val_one  = NULL;
static Game *val_up   = NULL;
static Game *val_down = NULL;
static Game *val_up_star   = NULL;
static Game *val_down_star = NULL;

static char print_buffer[65536];


void singletons_init(void) {
    // 0 = { | }
    val_zero = game_canonicalize(game_make(NULL, 0, NULL, 0));

    // * = { 0 | 0 }
    Game *arr_zero[] = { val_zero };
    val_star = game_canonicalize(game_make(arr_zero, 1, arr_zero, 1));

    // 1 = { 0 | }
    val_one = game_canonicalize(game_make(arr_zero, 1, NULL, 0));

    // ↑ = { 0 | * }
    Game *arr_star[] = { val_star };
    val_up = game_canonicalize(game_make(arr_zero, 1, arr_star, 1));

    // ↓ = { * | 0 }
    val_down = game_canonicalize(game_make(arr_star, 1, arr_zero, 1));

    // ↑* = ↑ + * = { 0, * | 0 }
    Game *arr_zero_star[] = { val_zero, val_star };
    val_up_star = game_canonicalize(game_make(arr_zero_star, 2, arr_zero, 1));

    // ↓* = ↓ + * = { 0 | 0, * }
    val_down_star = game_canonicalize(game_make(arr_zero, 1, arr_zero_star, 2));
}

// -----------------------------------------------------------------
// Gettery
// -----------------------------------------------------------------
Game* game_zero(void) { return val_zero; }
Game* game_star(void) { return val_star; }
Game* game_one(void)  { return val_one; }
Game* game_up(void)   { return val_up; }
Game* game_down(void) { return val_down; }


int is_zero(Game *G) { return G == val_zero; }
int is_star(Game *G) { return G == val_star; }
int is_one(Game *G)  { return G == val_one; }

// -----------------------------------------------------------------
// Násobky arrow
// -----------------------------------------------------------------
static int get_up_arrow_multiple(Game *G) {
    if (!G) return 0;
    int count = 1;
    Game *curr = G;

    while (1) {
        if (curr->L_count != 1 || curr->R_count != 1) return 0;

        if (curr->left[0] != val_zero) return 0;
        if (curr->right[0] == val_up_star) return count + 1;
        if (curr->right[0] == val_star) return count;

        curr = curr->right[0];
        count++;
    }
    return 0;
}

static int get_down_arrow_multiple(Game *G) {
    if (!G) return 0;
    int count = 1;
    Game *curr = G;

    while (1) {
        if (curr->L_count != 1 || curr->R_count != 1) return 0;

        if (curr->right[0] != val_zero) return 0;
        if (curr->left[0] == val_down_star) return count + 1;
        if (curr->left[0] == val_star) return count;

        curr = curr->left[0];
        count++;
    }
    return 0;
}


// -----------------------------------------------------------------
// Nimber funkce
// -----------------------------------------------------------------
// Vrací 'n' pro hodnotu *n (kde *0 = 0), nebo -1 pokud to není nim-hodnota
static int get_nimber_value(Game *G) {
    if (!G) return -1;
    if (G->L_count == 0 && G->R_count == 0) return 0; // Nula je *0

    // Nim-hodnota musí mít symetrický počet tahů
    if (G->L_count != G->R_count) return -1;

    int n = G->L_count;

    // Pole zda jsme našli všechny hodnoty 0 až n-1
    int *found_L = (int*)calloc(n, sizeof(int));
    int *found_R = (int*)calloc(n, sizeof(int));
    int is_nimber = 1;

    for (int i = 0; i < n; i++) {
        int vL = get_nimber_value(G->left[i]);
        int vR = get_nimber_value(G->right[i]);

        // Pokud potomek není nim-hodnota, nebo je moc velký, končíme
        if (vL < 0 || vL >= n) { is_nimber = 0; break; }
        if (vR < 0 || vR >= n) { is_nimber = 0; break; }

        found_L[vL] = 1;
        found_R[vR] = 1;
    }

    // Kontrola, zda nám nechybí žádná mezera (např. máme 0 a *2, ale chybí *1)
    if (is_nimber) {
        for (int i = 0; i < n; i++) {
            if (!found_L[i] || !found_R[i]) {
                is_nimber = 0;
                break;
            }
        }
    }

    free(found_L);
    free(found_R);

    return is_nimber ? n : -1;
}

// Naše sureální čísla nebudou mít tak velké zanoření
// (relativně podle limitů solveru max ≈500), takže rekurze není problém
// -----------------------------------------------------------------
// Pomocné funkce
// -----------------------------------------------------------------
// Vrací 1, pokud je kanonická hra dyadické/celé číslo
static int is_number(Game *G) {
    if (!G) return 0;
    if (G->L_count == 0 && G->R_count == 0) return 1; // 0 je číslo
    if (G->L_count > 1 || G->R_count > 1) return 0;   // Jakmile je tahů víc, není to číslo

    // Potomci musí být taky čísla
    if (G->L_count == 1 && !is_number(G->left[0])) return 0;
    if (G->R_count == 1 && !is_number(G->right[0])) return 0;

    return 1;
}

// Vrací 1, pokud je hra číslo, a uloží jeho hodnotu do *out_val
static int get_dyadic_value(Game *G, double *out_val) {
    if (!G) return 0;

    // base case
    if (G == game_zero()) {
        *out_val = 0.0;
        return 1;
    }

    // Kanonické číslo nesmí mít více než 1 tah na každou stranu
    // Toto funguje pouze kdyz je cislo v kanonicke forme
    // (v tomto pripade kdyz nema zadne dominovane tahy)
    if (G->L_count > 1 || G->R_count > 1) return 0;

    double l_val = 0.0, r_val = 0.0;
    int has_L = 0, has_R = 0;

    // Pokud má levý tah, musí to být také číslo
    if (G->L_count == 1) {
        if (!get_dyadic_value(G->left[0], &l_val)) return 0;
        has_L = 1;
    }
    // Pokud má pravý tah, musí to být také číslo
    if (G->R_count == 1) {
        if (!get_dyadic_value(G->right[0], &r_val)) return 0;
        has_R = 1;
    }

    // Výpočet hodnoty podle Conwayových pravidel pro čísla:
    if (has_L && !has_R) {
        *out_val = l_val + 1.0;  // Přičítání (např. {0 | } = 1)
        return 1;
    } else if (!has_L && has_R) {
        *out_val = r_val - 1.0;  // Odčítání (např. { | 0} = -1)
        return 1;
    } else if (has_L && has_R) {
        // V číslech musí platit, že levá možnost je menší než pravá
        if (l_val >= r_val) return 0;

        // Toto funguje pouze když je číslo v kanonickém tvaru
        // (v tomto pripade pokud nema zadne reverzibilni tahy)
        *out_val = (l_val + r_val) / 2.0; // Zlomek (např. {0 | 1} = 0.5)
        return 1;
    }

    return 0;
}


// Je hra přesně base + * ? (Tedy {base | base})
static int is_base_plus_star(Game *G, Game *base) {
    if (!G || !base) return 0;
    return (G->L_count == 1 && G->R_count == 1 &&
            G->left[0] == base && G->right[0] == base);
}

// Určitě jde nějak teoreticky dokázat proč všechny ty divné algoritmy
// fungují, ale byly vytvořeny jako vypsání pár prvních a najítí patternu
// -----------------------------------------------------------------
// 1. dyad. číslo + násobky šipky dolů (X + n↓)
// -----------------------------------------------------------------
static int get_number_plus_down_arrows(Game *G, double *out_base_val) {
    // Musí mít právě jeden tah na obě strany
    if (!G || G->L_count != 1 || G->R_count != 1) return 0;

    Game *base = G->right[0];
    if (!is_number(base)) return 0; // Základ musi být číslo

    int count = 1;
    Game *curr = G;

    while (1) {
        if (curr->L_count != 1 || curr->R_count != 1) return 0;
        if (curr->right[0] != base) return 0; // base musí zůstat stejná

        Game *next = curr->left[0]; // Zanořujeme se DOLEVA

        if (next->L_count == 1 && next->left[0] == base) {

            // a) next->right == base (To je {base | base}, tedy přesně base + *)
            if (next->R_count == 1 && next->right[0] == base) {
                get_dyadic_value(base, out_base_val);
                return count;
            }

            // b) next->right == {base, {base|base}}
            if (next->R_count == 2) {
                int has_base = (next->right[0] == base || next->right[1] == base);
                int has_star = (is_base_plus_star(next->right[0], base) || is_base_plus_star(next->right[1], base));
                if (has_base && has_star) {
                    get_dyadic_value(base, out_base_val);
                    return count + 1;
                }
            }
        }

        curr = next;
        count++;
    }
    return 0;
}



static int get_number_plus_up_arrows(Game *G, double *out_base_val) {
    // Musí mít právě jeden tah na obě strany
    if (!G || G->L_count != 1 || G->R_count != 1) return 0;

    // U šipky nahoru (↑ = {0 | *}) je základní číslo vzdy vlevo
    Game *base = G->left[0];
    if (!is_number(base)) return 0;

    int count = 1;
    Game *curr = G;

    while (1) {
        if (curr->L_count != 1 || curr->R_count != 1) return 0;
        if (curr->left[0] != base) return 0; // base je teď vlevo

        Game *next = curr->right[0]; // Zanořujeme se doprava

        if (next->R_count == 1 && next->right[0] == base) {

            // next->left == base (To je {base | base})
            if (next->L_count == 1 && next->left[0] == base) {
                get_dyadic_value(base, out_base_val);
                return count;
            }

            // next->left == {base, {base|base}}
            if (next->L_count == 2) {
                int has_base = (next->left[0] == base || next->left[1] == base);
                int has_star = (is_base_plus_star(next->left[0], base) || is_base_plus_star(next->left[1], base));
                if (has_base && has_star) {
                    get_dyadic_value(base, out_base_val);
                    return count + 1;
                }
            }
        }

        curr = next;
        count++;
    }
    return 0;
}


// -----------------------------------------------------------------
// Tisk
// -----------------------------------------------------------------
static void game_print_recursive(Game *G, enum output_format format) {
    if (!G) return;

    if (G == val_zero)      { printf("0"); return; }

    if (format == FORMAT_FORMATED) {
        double num_val;
        if (get_dyadic_value(G, &num_val)) {
            printf("%g", num_val);
            return;
        }

        // zakladni cisla
        if      (G == val_star) { printf("*"); return; }
        else if (G == val_one)  { printf("1"); return; }
        else if (G == val_up_star) { printf("↑*"); return; }
        else if (G == val_down_star) { printf("↓*"); return; }

        // násobky šipek
        int up_arrows = get_up_arrow_multiple(G);
        if (up_arrows > 0) {
            if (up_arrows == 1) printf("↑");
            else printf("%d↑", up_arrows);
            return;
        }

        int down_arrows = get_down_arrow_multiple(G);
        if (down_arrows > 0) {
            if (down_arrows == 1) printf("↓");
            else printf("%d↓", down_arrows);
            return;
        }

        // nasobky nimberu (*)
        int nimbers = get_nimber_value(G);
        if (nimbers > 0) {
            if (nimbers == 1) printf("*");
            else printf("*%d", nimbers);
            return;
        }

        // císlo + X šipek
        double base_val;

        int n_down = get_number_plus_down_arrows(G, &base_val);
        if (n_down > 0) {
            if (n_down == 1) printf("%g + ↓", base_val);
            else printf("%g + %d↓", base_val, n_down);
            return;
        }

        int n_up = get_number_plus_up_arrows(G, &base_val);
        if (n_up > 0) {
            if (n_up == 1) printf("%g + ↑", base_val);
            else printf("%g + %d↑", base_val, n_up);
            return;
        }

        // cislo + X nimberu
        if (G->L_count == 1 && G->R_count == 1 && G->left[0] == G->right[0]) {
            if(get_dyadic_value(G->left[0], &num_val)) {
                printf("%g + *", num_val);
                return;
            }
        }
    }

    // fallback
    printf("{");
    for (int i = 0; i < G->L_count; i++) {
        game_print_recursive(G->left[i], format);
        if (i < G->L_count - 1) printf(", ");
    }

    printf(" | ");
    for (int i = 0; i < G->R_count; i++) {
        game_print_recursive(G->right[i], format);
        if (i < G->R_count - 1) printf(", ");
    }
    printf("}");

}

void game_print(Game *G, enum output_format format) {
    if (!G) {
        printf("NULL\n");
        return;
    }
    game_print_recursive(G, format);
    printf("\n");
}



static void buffer_append(const char *text) {
    size_t current_len = strlen(print_buffer);
    size_t max_len = sizeof(print_buffer) - 1;
    if (current_len < max_len) {
        strncat(print_buffer, text, max_len - current_len);
    }
}

// Hodnotu surrealniho cisla zapise do bufferu
static void game_get_string_recursive(Game *G, enum output_format format) {
    if (!G) return;

    if (G == val_zero) { buffer_append("0"); return; }
    if (format == FORMAT_FORMATED) {
        char temp[64];

        double num_val;
        if (get_dyadic_value(G, &num_val)) {
            snprintf(temp, sizeof(temp), "%g", num_val);
            buffer_append(temp);
            return;
        }

        if      (G == val_star) { buffer_append("*"); return; }
        else if (G == val_one)  { buffer_append("1"); return; }
        else if (G == val_up_star)   { buffer_append("↑ + *"); return; }
        else if (G == val_down_star) { buffer_append("↓ + *"); return; }

        // násobky šipek
        int up_arrows = get_up_arrow_multiple(G);
        if (up_arrows > 0) {
            if (up_arrows == 1) snprintf(temp, sizeof(temp), "↑");
            else snprintf(temp, sizeof(temp), "%d↑", up_arrows);
            buffer_append(temp);
            return;
        }

        int down_arrows = get_down_arrow_multiple(G);
        if (down_arrows > 0) {
            if (down_arrows == 1) snprintf(temp, sizeof(temp), "↓");
            else snprintf(temp, sizeof(temp), "%d↓", down_arrows);
            buffer_append(temp);
            return;
        }

        // nasobky nimberu (*)
        int nimbers = get_nimber_value(G);
        if (nimbers > 0) {
            if (nimbers == 1) snprintf(temp, sizeof(temp), "*");
            else snprintf(temp, sizeof(temp), "*%d", nimbers);
            buffer_append(temp);
            return;
        }

        // císlo + X šipek
        double base_val;

        int n_down = get_number_plus_down_arrows(G, &base_val);
        if (n_down > 0) {
            if (n_down == 1) snprintf(temp, sizeof(temp), "%g + ↓", base_val);
            else snprintf(temp, sizeof(temp), "%g + %d↓", base_val, n_down);
            buffer_append(temp);
            return;
        }

        int n_up = get_number_plus_up_arrows(G, &base_val);
        if (n_up > 0) {
            if (n_up == 1) snprintf(temp, sizeof(temp), "%g + ↑", base_val);
            else snprintf(temp, sizeof(temp), "%g + %d↑", base_val, n_up);
            buffer_append(temp);
            return;
        }

        // cislo + X nimberu
        if (G->L_count == 1 && G->R_count == 1 && G->left[0] == G->right[0]) {
            if(get_dyadic_value(G->left[0], &num_val)) {
                snprintf(temp, sizeof(temp), "%g + *", num_val);
                buffer_append(temp);
                return;
            }
        }
    }

    // fallback
    buffer_append("{");
    for (int i = 0; i < G->L_count; i++) {
        game_get_string_recursive(G->left[i], format);
        if (i < G->L_count - 1) buffer_append(", ");
    }
    buffer_append(" | ");
    for (int i = 0; i < G->R_count; i++) {
        game_get_string_recursive(G->right[i], format);
        if (i < G->R_count - 1) buffer_append(", ");
    }
    buffer_append("}");
}

// wrapper pro python
const char* game_get_string(Game *G, enum output_format format) {
    print_buffer[0] = '\0';
    if (!G) return "NULL";
    game_get_string_recursive(G, format);
    return print_buffer;
}





/* ================================================================
   ADDITIONS — append this block to singletons.c
   Also replace the existing game_get_string() at the bottom with
   the new two-argument version below.
   ================================================================ */

/* -- make_int -------------------------------------------------------------- */
/* Build integer n as a canonical Game*. */
Game* make_int(int n) {
    if (n == 0) return game_zero();

    if (n > 0) {
        Game *prev  = make_int(n - 1);
        Game *arr[] = { prev };
        return game_canonicalize(game_make(arr, 1, NULL, 0));
    } else {
        Game *prev  = make_int(n + 1);
        Game *arr[] = { prev };
        return game_canonicalize(game_make(NULL, 0, arr, 1));
    }
}


/* -- make_dyadic ----------------------------------------------------------- */
/* Build dyadic rational p/q.  q must be a positive power of 2.
   Returns NULL on bad input. */
Game* make_dyadic(int p, int q) {
    if (q <= 0 || (q & (q - 1)) != 0) return NULL;   /* q not power of 2 */
    if (q == 1) return make_int(p);

    /* Reduce when numerator is even */
    if (p % 2 == 0) return make_dyadic(p / 2, q / 2);

    /* Odd numerator: { (p-1)/q | (p+1)/q } */
    Game *left  = make_dyadic(p - 1, q);
    Game *right = make_dyadic(p + 1, q);
    if (!left || !right) return NULL;

    Game *l_arr[] = { left  };
    Game *r_arr[] = { right };
    return game_canonicalize(game_make(l_arr, 1, r_arr, 1));
}


/* -- make_nimber ----------------------------------------------------------- */
/* Build nimber *n.  *0 = 0, *1 = *, *2 = {0,* | 0,*}, etc. */
Game* make_nimber(int n) {
    if (n < 0) return NULL;
    if (n == 0) return game_zero();
    if (n == 1) return game_star();

    Game **opts = (Game **)malloc((size_t)n * sizeof(Game *));
    if (!opts) return NULL;

    for (int i = 0; i < n; i++) opts[i] = make_nimber(i);

    Game *result = game_canonicalize(game_make(opts, n, opts, n));
    free(opts);
    return result;
}


/* -- make_up_multiple ------------------------------------------------------ */
/* Build n*↑, optionally adding * at the end.
   n must be >= 1.  with_star: 0 or 1. */
Game* make_up_multiple(int n, int with_star) {
    if (n <= 0) return NULL;

    Game *base   = game_up();
    Game *result = base;

    for (int i = 1; i < n; i++) result = game_add(result, base);
    if (with_star)               result = game_add(result, game_star());

    return result;
}


/* -- make_down_multiple ---------------------------------------------------- */
Game* make_down_multiple(int n, int with_star) {
    if (n <= 0) return NULL;

    Game *base   = game_down();
    Game *result = base;

    for (int i = 1; i < n; i++) result = game_add(result, base);
    if (with_star)               result = game_add(result, game_star());

    return result;
}
