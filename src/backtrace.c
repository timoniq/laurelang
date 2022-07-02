#include "laurelang.h"

#ifndef BACKTRACE_CHAIN_LIMIT
#define BACKTRACE_CHAIN_LIMIT 10
#endif

/* Backtrace
    log - current evaluated expression info
    chain - chain of keypoints (limited by BACKTRACE_CHAIN_LIMIT)
*/

laure_expression_type BACKTRACED_EXPRESSIONS[] = {let_pred_call, let_choice_2, let_unify, let_imply};

bool is_backtraced(laure_expression_type t) {
    for (uint i = 0; i < sizeof(BACKTRACED_EXPRESSIONS) / sizeof(laure_expression_type); i++) {
        if (t == BACKTRACED_EXPRESSIONS[i])
            return true;
    }
    return false;
}

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

void laure_backtrace_add(laure_backtrace *backtrace, laure_expression_t *e) {
    if (! backtrace) return;
    if (! e || ! is_backtraced(e->t)) return;
    if (backtrace->cursor >= BACKTRACE_CHAIN_LIMIT - 2) {
        struct chain_p nchain[BACKTRACE_CHAIN_LIMIT-1];
        memcpy(nchain, backtrace->chain + backtrace->cursor - BACKTRACE_CHAIN_LIMIT + 1, sizeof(struct chain_p) * (BACKTRACE_CHAIN_LIMIT - 1));
        memset(backtrace->chain, 0, BACKTRACE_CHAIN_CAPACITY * sizeof(struct chain_p));
        memcpy(backtrace->chain, nchain, (BACKTRACE_CHAIN_LIMIT-1) * sizeof(struct chain_p));
        backtrace->cursor = BACKTRACE_CHAIN_LIMIT-1;
    }
    if (backtrace->cursor && str_eq(backtrace->chain[backtrace->cursor-1].e->fullstring, e->fullstring)) {
        backtrace->chain[backtrace->cursor].times++;
    } else {
        struct chain_p p;
        p.e = e;
        p.times = 0;
        backtrace->chain[backtrace->cursor++] = p;
    }
}
// pretty backtrace print
// predcall
// 
void laure_backtrace_print(laure_backtrace *backtrace) {
    if (! backtrace) return;
    struct chain_p *chain = backtrace->chain;
    if (chain[0].e) {
        printf("\n  ━━ Backtrace ━━\n");
    }
    uint i = 0;
    while (chain[0].e && i <= backtrace->cursor) {
        struct chain_p p = chain[0];
        printf("  %s↪%s because of %s:\n", GREEN_COLOR, NO_COLOR, EXPT_NAMES[p.e->t]);
        if (p.times) printf("    %s%s%s (%u times)\n", GRAY_COLOR, p.e->fullstring, NO_COLOR, p.times);
        else printf("      %s%s%s\n", GRAY_COLOR, p.e->fullstring, NO_COLOR);
        chain++;
        i++;
    }
    if (backtrace->chain != chain) {
        printf("  ━━━━━━━━━━━━━━━\n\n");
    }
    if (backtrace->log)
        printf("in %s\n", backtrace->log);
}