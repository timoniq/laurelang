#include "laurelang.h"

#ifndef BACKTRACE_CHAIN_LIMIT
#define BACKTRACE_CHAIN_LIMIT 10
#endif

/* Backtrace
    log - current evaluated expression info
    chain - chain of keypoints (limited by BACKTRACE_CHAIN_LIMIT)
*/

laure_backtrace laure_backtrace_new() {
    laure_backtrace bt;
    bt.cursor = 0;
    bt.log = NULL;
    memset(bt.chain, 0, (BACKTRACE_CHAIN_CAPACITY-1) * sizeof(struct chain_p));
    return bt;
}

void laure_backtrace_log(laure_backtrace *backtrace, string log) {
    if (! backtrace) return;
    backtrace->log = log;
}

void laure_backtrace_nullify(laure_backtrace *backtrace) {
    if (! backtrace) return;
    memset(backtrace->chain, 0, (BACKTRACE_CHAIN_CAPACITY-1) * sizeof(struct chain_p));
    backtrace->cursor = 0;
}

void laure_backtrace_add(laure_backtrace *backtrace, string key) {
    if (! backtrace) return;
    if (! key) return;
    if (backtrace->cursor >= BACKTRACE_CHAIN_CAPACITY - 2) {
        struct chain_p nchain[BACKTRACE_CHAIN_LIMIT-1];
        memcpy(nchain, backtrace->chain + backtrace->cursor - BACKTRACE_CHAIN_LIMIT + 1, sizeof(struct chain_p) * (BACKTRACE_CHAIN_LIMIT - 1));
        memset(backtrace->chain, 0, BACKTRACE_CHAIN_CAPACITY * sizeof(struct chain_p));
        memcpy(backtrace->chain, nchain, (BACKTRACE_CHAIN_LIMIT-1) * sizeof(struct chain_p));
        backtrace->cursor = BACKTRACE_CHAIN_LIMIT-1;
    }
    if (backtrace->cursor && str_eq(backtrace->chain[backtrace->cursor-1].key, key)) {
        backtrace->chain[backtrace->cursor].times++;
    } else {
        struct chain_p p;
        p.key = key;
        p.times = 0;
        backtrace->chain[backtrace->cursor++] = p;
    }
}

void laure_backtrace_print(laure_backtrace *backtrace) {
    if (! backtrace) return;
    struct chain_p *chain = backtrace->chain;
    while (chain[0].key) {
        struct chain_p p = chain[0];
        if (p.times) printf("%s (%u times)\n", p.key, p.times);
        else printf("%s\n", p.key);
        chain++;
    }
    if (backtrace->log)
        printf("in %s\n", backtrace->log);
}