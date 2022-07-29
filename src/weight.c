#ifndef DISABLE_WS

#include "laurelang.h"
#include <stdlib.h>
#include <math.h>

typedef struct laure_ws laure_ws;
typedef float optimality_t;

#define INITIAL_CAPACITY 16

double k       = 0.01;
double align_f = 10;
double e       = 2.71828;

double sigmoid(laure_ws *ws, size_t i) {
    return 1e6 / (1 + powf(e, (i + align_f) * k));
}

double constant(laure_ws *ws, size_t i) {
    return 1e4;
}

double (*DEFAULT_WSFUNC)(laure_ws*, size_t) = sigmoid;

typedef struct laure_optimality_vector {
    size_t capacity;
    size_t length;
    optimality_t *ptr;
} laure_optimality_vector;

laure_optimality_vector *vector_init() {
    laure_optimality_vector *vec = laure_alloc(sizeof(laure_optimality_vector));
    vec->capacity = INITIAL_CAPACITY;
    vec->length = 0;
    vec->ptr = laure_alloc(sizeof(void*) * vec->capacity);
    return vec;
}

void vector_expand(laure_optimality_vector *vec) {
    vec->capacity = vec->capacity * 2;
    vec->ptr = laure_realloc(vec->ptr, vec->capacity);
}

void vector_add(laure_optimality_vector *vec, optimality_t optim) {
    if (vec->length == vec->capacity) {
        vector_expand(vec);
    }
    vec->ptr[vec->length++] = optim;
}

void vector_free(laure_optimality_vector* vec) {
    laure_free(vec->ptr);
    laure_free(vec);
} 

typedef struct laure_ws {
    double k;
    laure_optimality_vector *vec;
    double (*calc_w)(laure_ws*, size_t);
    laure_ws *next;
} laure_ws;

laure_ws *laure_ws_create(laure_ws *next) {
    laure_ws *ws = laure_alloc(sizeof(laure_ws));
    ws->next = next;
    ws->calc_w = next ? next->calc_w : DEFAULT_WSFUNC;
    ws->k = k;
    ws->vec = vector_init();
    return ws;
}

void laure_ws_free(laure_ws *ws) {
    vector_free(ws->vec);
    laure_free(ws);
}

optimality_t round_o(optimality_t o) {
    if (o < 0.001) return 0;
    return o;
}

optimality_t laure_ws_soften(optimality_t o) {
    /* mock for now
    later some function to
    soften final optimality */
    return o;
}

void print_transistions(laure_ws *ws) {
    for (uint i = 0; i < ws->vec->length; i++)
        printf("%f ", ws->vec->ptr[i]);
    printf("\n");
}

optimality_t laure_optimality_count(
    laure_ws *ws
) {
    #ifdef DEBUG
    print_transistions(ws);
    #endif

    weight_t F = 0;
    weight_t Fd = 0;

    for (size_t i = 0; i < ws->vec->length; i++) {
        optimality_t a = ws->vec->ptr[i];
        double w = ws->calc_w(ws, i);
        F += w;
        Fd += w * a;
    }

    if (F == 0) return 1;

    optimality_t o = (optimality_t)((float)Fd / (float)F);
    o = round_o(o);
    #ifdef DEBUG
    printf("==> %f\n", o);
    #endif
    return o;
}

laure_ws *laure_ws_next(laure_ws *ws) {
    if (! ws) return 0;
    return ws->next;
}

void laure_push_transistion(laure_ws *ws, optimality_t optim) {
    vector_add(ws->vec, optim);
}

size_t laure_count_transistions(laure_ws *ws) {
    return ws->vec->length;
}

void laure_restore_transistions(laure_ws *ws, size_t to_sz) {
    ws->vec->length = to_sz;
}

bool laure_ws_set_function_by_name(laure_ws *ws, string name) {
    if (str_eq(name, "sigmoid")) {
        ws->calc_w = sigmoid;
    } else if (str_eq(name, "constant")) {
        ws->calc_w = constant;
    } else {
        return false;
    }
    DEFAULT_WSFUNC = ws->calc_w;
    return true;
}

#endif