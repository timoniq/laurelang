#ifndef DISABLE_WS

#include <vector>
#include <math.h>

// weighted search algorithms
// by Arseny Kriuchkov
// read paper here: https://docs.laurelang.org/wiki/ws

typedef struct laure_ws laure_ws;
typedef unsigned long weight_t;
typedef float accuracy_t;

typedef unsigned long weight_t;
typedef float accuracy_t;
weight_t k = 1e5;
double e = 2.718;

typedef struct laure_ws {
    weight_t k = k;
    std::vector<accuracy_t> acc;
    weight_t (*antiderivative)(laure_ws*, size_t);
} laure_ws;

weight_t calculate_antiderivative(laure_ws *ws, size_t i) {
    return (1/2) * (1 / pow(e, ws->k * (i + 1)));
}

weight_t integrate(laure_ws *ws, size_t from, size_t to) {
    return ws->antiderivative(ws, to) - ws->antiderivative(ws, from);
}

accuracy_t laure_accuracy_count(
    laure_ws *ws
) {
    weight_t F = integrate(ws, 0, ws->acc.size());
    weight_t Fd = 0;
    for (size_t i = 0; i < ws->acc.size(); i++) {
        accuracy_t a = ws->acc.at(i);
        weight_t w = integrate(ws, i, i + 1);
        Fd += w * a;
    }
    return (accuracy_t)(Fd / F);
} 

laure_ws *laure_ws_create() {
    std::vector<accuracy_t> acc;
    laure_ws *ws = (laure_ws*)malloc(sizeof(laure_ws));
    ws->acc = acc;
    ws->antiderivative = calculate_antiderivative;
    ws->k = k;
    return ws;
}

void laure_push_transistion(laure_ws *ws, accuracy_t acc) {
    ws->acc.push_back(acc);
}
#endif