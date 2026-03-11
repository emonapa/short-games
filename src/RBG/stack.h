#ifndef STACK_H
#define STACK_H

#include <stddef.h>
#include <stdbool.h>
#include "error.h"


//TODO

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
void *Pop(TStack *stack);          // vraci pointer na popped prvek (pouzij hned)

bool IsEmpty(TStack *stack);

#endif // STACK_H
