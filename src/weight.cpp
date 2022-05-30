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
typedef float accuracy_t;

double k = 0.000001;
double e = 2.71828;

extern "C" {
    laure_ws *laure_ws_create(laure_ws *next);
    void laure_ws_free(laure_ws *ws);
    accuracy_t laure_accuracy_count(
        laure_ws *ws
    );
    laure_ws *laure_ws_next(laure_ws *ws);
    void laure_push_transistion(laure_ws *ws, accuracy_t acc);
    size_t laure_count_transistions(laure_ws *ws);
    void laure_restore_transistions(laure_ws *ws, size_t to_sz);
}

typedef struct laure_ws {
    double k = k;
    std::vector<accuracy_t> *acc;
    double (*antiderivative)(laure_ws*, size_t);
    laure_ws *next;
} laure_ws;

double calculate_antiderivative(laure_ws *ws, size_t i) {
    return (i * ws->k) - log(pow(e, (i * ws->k)) + 1) - (i * ws->k);
}

double integrate(laure_ws *ws, size_t from, size_t to) {
    return ws->antiderivative(ws, to) - ws->antiderivative(ws, from);
}

double round_a(double a) {
    if (a > 0.999) return 1;
    else if (a < 0.001) return 0;
    return a;
}

accuracy_t laure_accuracy_count(
    laure_ws *ws
) {
    weight_t F = integrate(ws, 0, ws->acc->size());
    if (! F) return 1;
    weight_t Fd = 0;
    for (size_t i = 0; i < ws->acc->size(); i++) {
        accuracy_t a = round_a(ws->acc->at(i));
        double w = integrate(ws, i, i + 1);
        Fd += w * a;
    }
    accuracy_t a = (accuracy_t)(Fd / F);
    return round_a(a);
} 

laure_ws *laure_ws_create(laure_ws *next) {
    std::vector<accuracy_t> *acc = new std::vector<accuracy_t>();

    laure_ws *ws = (laure_ws*) malloc(sizeof(laure_ws));
    ws->acc  = acc;
    ws->antiderivative = calculate_antiderivative;
    ws->k    = k;
    ws->next = next;
    return ws;
}

void laure_push_transistion(laure_ws *ws, accuracy_t acc) {
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