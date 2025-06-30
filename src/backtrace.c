// Backtrace storage

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

// Check if predicate should be filtered from reasoning chain
static bool is_internal_predicate(const char* predicate_name) {
    if (!predicate_name) return true;
    
    // Filter out internal predicates
    if (strstr(predicate_name, "__") != NULL) return true;  // Double underscore internals
    if (strstr(predicate_name, " __") != NULL) return true; // Space + double underscore
    if (strstr(predicate_name, " +") != NULL) return true;  // Arithmetic operators
    if (strstr(predicate_name, " -") != NULL) return true;
    if (strstr(predicate_name, " *") != NULL) return true;
    if (strstr(predicate_name, " /") != NULL) return true;
    if (strstr(predicate_name, " =") != NULL) return true;  // Unification operators
    if (strstr(predicate_name, " <") != NULL) return true;  // Comparison operators
    if (strstr(predicate_name, " >") != NULL) return true;
    if (strstr(predicate_name, "abstract_") != NULL) return true; // Abstract internals
    
    return false;
}

void laure_backtrace_add(laure_backtrace *backtrace, laure_expression_t *e) {
    if (! backtrace) return;
    if (! e || ! is_backtraced(e->t)) return;
    
    // Filter out internal predicates from reasoning chain
    if (e->fullstring && is_internal_predicate(e->fullstring)) {
        return;
    }
    
    if (backtrace->cursor >= BACKTRACE_CHAIN_LIMIT - 2) {
        struct chain_p nchain[BACKTRACE_CHAIN_LIMIT-1];
        memcpy(nchain, backtrace->chain + backtrace->cursor - BACKTRACE_CHAIN_LIMIT + 1, sizeof(struct chain_p) * (BACKTRACE_CHAIN_LIMIT - 1));
        memset(backtrace->chain, 0, BACKTRACE_CHAIN_CAPACITY * sizeof(struct chain_p));
        memcpy(backtrace->chain, nchain, (BACKTRACE_CHAIN_LIMIT-1) * sizeof(struct chain_p));
        backtrace->cursor = BACKTRACE_CHAIN_LIMIT-1;
    }
    if (e->fullstring) {
        if (backtrace->cursor && backtrace->chain[backtrace->cursor-1].e && str_eq(backtrace->chain[backtrace->cursor-1].e->fullstring, e->fullstring)) {
            backtrace->chain[backtrace->cursor-1].times++;
        } else {
            struct chain_p p;
            p.e = e;
            p.times = 0;
            backtrace->chain[backtrace->cursor++] = p;
        }
    }
}

// Enhanced reasoning chain display for logic programming
void laure_backtrace_print(laure_backtrace *backtrace) {
    if (! backtrace) return;
    struct chain_p *chain = backtrace->chain;
    
    if (!chain[0].e) {
        printf("  %sNo user predicates in reasoning chain%s\n", GRAY_COLOR, NO_COLOR);
        if (backtrace->log) {
            printf("  %sContext: %s%s\n", GRAY_COLOR, backtrace->log, NO_COLOR);
        }
        return;
    }
    
    printf("  %sReasoning chain:%s\n", BOLD_DEC, NO_COLOR);
    
    uint i = 0;
    while (chain[0].e && i < backtrace->cursor) {
        struct chain_p p = chain[0];
        
        // Choose appropriate reasoning symbol based on expression type
        const char* symbol = "├─";
        const char* color = BLUE_COLOR;
        const char* action = "";
        
        switch (p.e->t) {
            case let_pred_call:
                symbol = "│ ⊢";
                color = GREEN_COLOR;
                action = "invoked";
                break;
            case let_unify:
                symbol = "│ ≡";
                color = YELLOW_COLOR;
                action = "unified";
                break;
            case let_imply:
                symbol = "│ →";
                color = BLUE_COLOR;
                action = "deduced";
                break;
            case let_choice_2:
                symbol = "│ ∨";
                color = PURPLE_COLOR;
                action = "branched";
                break;
            default:
                symbol = "│ •";
                color = GRAY_COLOR;
                action = "processed";
                break;
        }
        
        // Last item gets different symbol
        if (i == backtrace->cursor - 1 || !chain[1].e) {
            if (strstr(symbol, "⊢")) symbol = "└─⊢";
            else if (strstr(symbol, "≡")) symbol = "└─≡";
            else if (strstr(symbol, "→")) symbol = "└─→";
            else if (strstr(symbol, "∨")) symbol = "└─∨";
            else symbol = "└─•";
        }
        
        printf("  %s%s%s %s %s%s%s", color, symbol, NO_COLOR, action, BOLD_DEC, p.e->fullstring, NO_COLOR);
        
        if (p.times > 0) {
            printf(" %s(×%u)%s", GRAY_COLOR, p.times + 1, NO_COLOR);
        }
        printf("\n");
        
        chain++;
        i++;
    }
    
    if (backtrace->log) {
        printf("  %s╰─ context: %s%s\n", GRAY_COLOR, backtrace->log, NO_COLOR);
    }
    printf("\n");
}