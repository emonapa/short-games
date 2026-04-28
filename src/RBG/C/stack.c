#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "error.h"

#include "config.h"
#include "stack.h"

size_t stack_items_count = 0;

static void stack_grow(TStack *stack, size_t min_cap) {
    size_t new_cap = (stack->cap == 0) ? 64 : stack->cap;
    while (new_cap < min_cap) new_cap *= 2;

    size_t bytes = new_cap * stack->elem_size;
    void *p = realloc(stack->data, bytes);
    if (!p) {
        error_exit(ERR_MALLOC, "Realloc failed.\n");
    }

    stack->data = (unsigned char *)p;
    stack->cap = new_cap;
    stack_items_count = new_cap;
}

void stack_init(TStack *stack, size_t elem_size) {
    if (stack == NULL) error_exit(ERR_NULL_POINTER, "");
    if (elem_size == 0) error_exit(ERR_OTHER, "Trying to intialize stack to 0 elements");

    stack->data = NULL;
    stack->size = 0;
    stack->cap = 0;
    stack->elem_size = elem_size;
}

void stack_dtor(TStack *stack) {
    if (stack == NULL) {
        warning("Trying to free stack with null pointer to stack.\n");
        return;
    }
    free(stack->data);
    stack->data = NULL;
    stack->size = 0;
    stack->cap = 0;
    stack->elem_size = 0;
}

bool IsEmpty(TStack *stack) {
    if (stack == NULL) {
        warning("Calling stack function with null pointer to stack.\n");
        return true;
    }
    return stack->size == 0;
}

void Push(TStack *stack, const void *elem) {
    if (!stack || !elem) error_exit(ERR_NULL_POINTER, "");

    if (stack->size == stack->cap) {
        stack_grow(stack, stack->size + 1);
    }

    unsigned char *dst = stack->data + stack->size * stack->elem_size;
    memcpy(dst, elem, stack->elem_size);
    stack->size++;
}

void *Top(TStack *stack) {
    if (IsEmpty(stack)) error_exit(ERR_EMPTY_STACK, "Trying to get top of stack on empty stack.\n");

    return stack->data + (stack->size - 1) * stack->elem_size;
}

/*
  Vraci pointer na prvek, ktery byl prave odstraneny (data tam fyzicky zustanou).
  Tento pointer je validni do dalsiho Push(), ktere muze udelat realloc.
  Pouzij ho hned, neukladej ho.
*/
void *Pop(TStack *stack) {
    if (IsEmpty(stack)) error_exit(ERR_EMPTY_STACK, "Trying to pop an empty stack.\n");

    stack->size--;
    return stack->data + stack->size * stack->elem_size;
}
