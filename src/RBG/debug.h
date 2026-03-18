#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

// výběr formátu podle typu
#define DBG_FMT(x) _Generic((x), \
    int: "%d", \
    long: "%ld", \
    long long: "%lld", \
    unsigned: "%u", \
    float: "%f", \
    double: "%f", \
    char: "%c", \
    char*: "%s", \
    const char*: "%s", \
    default: "%p" \
)

// jeden argument
#define DBG_ONE(x) printf(#x "=" DBG_FMT(x), (x))

// pomocné makro pro čárku
#define DBG_COMMA() printf(", ")

// rozbalení až pro N argumentů (rozšiř dle potřeby)
#define DBG_1(a) DBG_ONE(a)
#define DBG_2(a,b) DBG_ONE(a); DBG_COMMA(); DBG_ONE(b)
#define DBG_3(a,b,c) DBG_ONE(a); DBG_COMMA(); DBG_ONE(b); DBG_COMMA(); DBG_ONE(c)
#define DBG_4(a,b,c,d) DBG_ONE(a); DBG_COMMA(); DBG_ONE(b); DBG_COMMA(); DBG_ONE(c); DBG_COMMA(); DBG_ONE(d)
#define DBG_5(a,b,c,d,e) DBG_ONE(a); DBG_COMMA(); DBG_ONE(b); DBG_COMMA(); DBG_ONE(c); DBG_COMMA(); DBG_ONE(d); DBG_COMMA(); DBG_ONE(e)

// výběr podle počtu argumentů
#define GET_MACRO(_1,_2,_3,_4,_5,NAME,...) NAME

#define DBG(...) do { \
    GET_MACRO(__VA_ARGS__, DBG_5, DBG_4, DBG_3, DBG_2, DBG_1)(__VA_ARGS__); \
    printf("\n"); \
} while(0)

#endif // DEBUG_H
