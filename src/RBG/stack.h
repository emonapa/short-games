#ifndef STACK_H
#define STACK_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    unsigned char *data;   // interni buffer
    size_t size;           // pocet prvku
    size_t cap;            // kapacita v poctu prvku
    size_t elem_size;      // velikost jednoho prvku v bajtech
} TStack;

void stack_init(TStack *stack, size_t elem_size);
void stack_dtor(TStack *stack);

void Push(TStack *stack, const void *elem);
void *Top(TStack *stack);

/*
 * =================================
 * |     DŮLEŽITÉ / IMPORTNANT     |
 * =================================
 * Pop vrací pointer na popped prvek,
 * ale po funkci Push, NENÍ DEFINOVANÉ co v něm je.
 *
 * Je to kvůli tomu, že po zavolání Push se může stack realokovat
 * a tím pádem bude pointer prvku ukazovat na nevalidní paměť.
 *
 * Toto je kompromis mezi rychlostí a použitelností.
 */
void *Pop(TStack *stack);          // vraci pointer na popped prvek

bool IsEmpty(TStack *stack);

#endif // STACK_H
