#ifndef DISABLE_WS

#include <vector>
#include <math.h>
#include <iterator>
#include <iostream>

// weighted search algorithms
// by Arseny Kriuchkov
// read paper here: https://docs.laurelang.org/wiki/ws

typedef struct laure_ws laure_ws;
typedef double weight_t;
typedef float optimality_t;

double k = 0.00001;
double align = 0;
double e = 2.718281828459045;

extern "C" {
    laure_ws *laure_ws_create(laure_ws *next);
    void laure_ws_free(laure_ws *ws);
    optimality_t laure_accuracy_count(
        laure_ws *ws
    );
    laure_ws *laure_ws_next(laure_ws *ws);
    void laure_push_transistion(laure_ws *ws, optimality_t acc);
    size_t laure_count_transistions(laure_ws *ws);
    void laure_restore_transistions(laure_ws *ws, size_t to_sz);
    optimality_t laure_ws_soften(optimality_t o);
}

typedef struct laure_ws {
    double k = k;
    std::vector<optimality_t> *acc;
    double (*antiderivative)(laure_ws*, size_t);
    laure_ws *next;
} laure_ws;

void print_transistions(laure_ws *ws) {
    std::vector<optimality_t> *v = ws->acc;
    std::copy(v->begin(), v->end(),
        std::ostream_iterator<optimality_t>(std::cout, " "));
}

double calculate_antiderivative(laure_ws *ws, size_t i) {
    return (i * ws->k + align) - log(pow(e, (i * ws->k + align)) + 1);
}

double integrate(laure_ws *ws, size_t from, size_t to) {
    return ws->antiderivative(ws, to) - ws->antiderivative(ws, from);
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

optimality_t laure_accuracy_count(
    laure_ws *ws
) { 
    #ifdef DEBUG
    print_transistions(ws);
    #endif
    weight_t F = integrate(ws, 0, ws->acc->size());
    if (! F) return 1;
    weight_t Fd = 0;
    for (size_t i = 0; i < ws->acc->size(); i++) {
        optimality_t a = round_o(ws->acc->at(i));
        double w = integrate(ws, i, i + 1);
        Fd += w * a;
    }
    optimality_t o = (optimality_t)(Fd / F);
    o = round_o(o);
    #ifdef DEBUG
    printf("==> %f\n", o);
    #endif
    return o;
} 

laure_ws *laure_ws_create(laure_ws *next) {
    std::vector<optimality_t> *acc = new std::vector<optimality_t>();

    laure_ws *ws = (laure_ws*) malloc(sizeof(laure_ws));
    ws->acc  = acc;
    ws->antiderivative = calculate_antiderivative;
    ws->k    = k;
    ws->next = next;
    return ws;
}

void laure_push_transistion(laure_ws *ws, optimality_t acc) {
    ws->acc->push_back(acc);
}

size_t laure_count_transistions(laure_ws *ws) {
    return ws->acc->size();
}

void laure_restore_transistions(laure_ws *ws, size_t to_sz) {
    ws->acc->resize(to_sz);
}

laure_ws *laure_ws_next(laure_ws *ws) {
    if (! ws) return 0;
    return ws->next;
}

void laure_ws_free(laure_ws *ws) {
    if (! ws) return;
    delete ws->acc;
    free(ws);
}
#endif