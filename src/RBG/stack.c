#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "stack.h"

size_t stack_items_count = 0;

static void stack_grow(TStack *stack, size_t min_cap) {
    size_t new_cap = (stack->cap == 0) ? 64 : stack->cap;
    while (new_cap < min_cap) new_cap *= 2;

    size_t bytes = new_cap * stack->elem_size;
    void *p = realloc(stack->data, bytes);
    if (!p) {
        error_exit(ERR_MALLOC, "Malloc/Realloc error in stack_grow [stack.c]\n");
    }

    stack->data = (unsigned char *)p;
    stack->cap = new_cap;
    stack_items_count = new_cap;
}

void stack_init(TStack *stack, size_t elem_size) {
    if (!stack || elem_size == 0) {
        fprintf(stderr, "Error: stack_init invalid args\n");
        exit(EXIT_FAILURE);
    }
    stack->data = NULL;
    stack->size = 0;
    stack->cap = 0;
    stack->elem_size = elem_size;
}

void stack_dtor(TStack *stack) {
    if (!stack) return;
    free(stack->data);
    stack->data = NULL;
    stack->size = 0;
    stack->cap = 0;
    stack->elem_size = 0;
}

bool IsEmpty(TStack *stack) {
    return (stack == NULL || stack->size == 0);
}

void Push(TStack *stack, const void *elem) {
    if (!stack || !elem) {
        fprintf(stderr, "Error: Push invalid args\n");
        exit(EXIT_FAILURE);
    }
    if (stack->size == stack->cap) {
        stack_grow(stack, stack->size + 1);
    }

    unsigned char *dst = stack->data + stack->size * stack->elem_size;
    memcpy(dst, elem, stack->elem_size);
    stack->size++;
}

void *Top(TStack *stack) {
    if (IsEmpty(stack)) {
        fprintf(stderr, "Error: Top operation on empty stack\n");
        exit(EXIT_FAILURE);
    }
    return stack->data + (stack->size - 1) * stack->elem_size;
}

/*
  Vraci pointer na prvek, ktery byl prave odstraneny (data tam fyzicky zustanou).
  Tento pointer je validni do dalsiho Push(), ktere muze udelat realloc.
  Pouzij ho hned, neukladej ho.
*/
void *Pop(TStack *stack) {
    if (IsEmpty(stack)) {
        fprintf(stderr, "Error: Pop operation on empty stack\n");
        exit(EXIT_FAILURE);
    }
    stack->size--;
    return stack->data + stack->size * stack->elem_size;
}
