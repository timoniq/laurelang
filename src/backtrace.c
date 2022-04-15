#include "laurelang.h"

#ifndef BACKTRACE_CHAIN_LIMIT
#define BACKTRACE_CHAIN_LIMIT 10
#endif

#define BACKTRACE_CHAIN_CAPACITY 100

/* Backtrace
    log - current evaluated expression info
    chain - chain of keypoints (limited by BACKTRACE_CHAIN_LIMIT)
*/

typedef struct laure_backtrace {
    string log;
    char *chain[BACKTRACE_CHAIN_CAPACITY];
    uint cursor;
} laure_backtrace;

void laure_backtrace_log(laure_backtrace *backtrace, string log) {
    backtrace->log = log;
}

void laure_backtrace_add(laure_backtrace *backtrace, string key) {
    if (backtrace->cursor >= BACKTRACE_CHAIN_CAPACITY - 2) {
        char *nchain[BACKTRACE_CHAIN_LIMIT-1];
        memcpy(nchain, backtrace->chain + backtrace->cursor - BACKTRACE_CHAIN_LIMIT + 1, sizeof(void*) * (BACKTRACE_CHAIN_LIMIT - 1));
        memset(backtrace->chain, 0, BACKTRACE_CHAIN_CAPACITY);
        memcpy(backtrace->chain, nchain, BACKTRACE_CHAIN_LIMIT-1);
        backtrace->cursor = BACKTRACE_CHAIN_LIMIT-1;
    }
    backtrace->chain[backtrace->cursor++] = key;
}

void laure_backtrace_print(laure_backtrace *backtrace) {
    char **chain = backtrace->chain;
    while((chain + backtrace->cursor)[0]) {
        char *key = (chain + backtrace->cursor)[0];
        printf("%s\n", key);
        chain++;
    }
    printf("in %s\n", backtrace->log);
}