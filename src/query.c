#include "laurelang.h"
#include "laureimage.h"
#include "predpub.h"
#include "memguard.h"

#include <readline/readline.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <math.h>

#define YIELD_OK (void*)1
#define YIELD_FAIL (void*)0
#define ANONVAR_NAME "_"

#ifndef WMIN_NAME
#define WMIN_NAME "ws_min"
#endif

#ifndef WFUNCTION_NAME
#define WFUNCTION_NAME "ws_func"
#endif

#define RECURSIVE_AUTONAME "_"

#define is_global(stack) stack->glob == stack
#define absorb(a, b) do {if (a > b) { a = a - b; b = 0; } else { b = b - a; a = 0; }; } while (0)
#ifndef DISABLE_WS
#define further_with_decision_accuracy(A) do { \
    if (! LAURE_WS) \
        if (A < 1) return RESPOND_FALSE; \
    else \
        laure_push_transistion(cctx->ws, A); \
    } while (0)
#else
#define further_with_decision_accuracy(A) do {return RESPOND_FALSE;} while (0)
#endif

int __x = 0;

#define isolated if (true) 

char *RESPN = NULL;
char *CONTN = NULL;
char *MARKER_NODELETE = NULL;
char *DUMMY_FLAGV = NULL;
char *ARGN_BUFF[32];
bool IS_BUFFN_INITTED = 0;
qcontext *LAST_QCTX = NULL;


// Global registry for name instantiation observers
struct name_observer_registry {
    unsigned long *name_links;     // Array of name links being observed
    void **observers;              // Corresponding data structures observing each name
    uint count;                    // Number of active observations
    uint capacity;                 // Allocated capacity
} g_name_observers = {NULL, NULL, 0, 0};

// Register any data structure to observe a name link
void register_name_observer(unsigned long name_link, void *observer) {
    if (g_name_observers.count >= g_name_observers.capacity) {
        // Expand registry
        uint new_capacity = g_name_observers.capacity == 0 ? 8 : g_name_observers.capacity * 2;
        g_name_observers.name_links = laure_realloc(g_name_observers.name_links, 
                                                   sizeof(unsigned long) * new_capacity);
        g_name_observers.observers = laure_realloc(g_name_observers.observers, 
                                                  sizeof(void*) * new_capacity);
        g_name_observers.capacity = new_capacity;
    }
    
    g_name_observers.name_links[g_name_observers.count] = name_link;
    g_name_observers.observers[g_name_observers.count] = observer;
    g_name_observers.count++;
}

// Notify all observers when a name gets instantiated
void notify_name_observers(unsigned long name_link, Instance *name_instance) {
    for (uint i = 0; i < g_name_observers.count; i++) {
        if (g_name_observers.name_links[i] == name_link) {
            void *observer = g_name_observers.observers[i];
            if (observer) {
                // Check if this is a solution-collecting array
                laure_image_head head = read_head(observer);
                if (head.t == ARRAY) {
                    struct ArrayImage *array = (struct ArrayImage*)observer;
                    if (array->is_solution_collector) {
                        // Collect current solution from all observed names
                        collect_solution_from_array(array);
                    }
                }
                // Future: Add support for other data structures that observe name instantiation
            }
        }
    }
}

// Collect a solution tuple from the current state of observed names
void collect_solution_from_array(struct ArrayImage *array) {
    if (!array || !array->is_solution_collector) return;
    
    // Check if all observed names are instantiated
    bool all_instantiated = true;
    for (uint i = 0; i < array->c_data.observed_count; i++) {
        Instance *name = array->c_data.observed_vars[i];
        if (!name || !name->image) {
            all_instantiated = false;
            break;
        }
        // For integers, check if state is I (instantiated)
        struct ImageHead head = read_head(name->image);
        if (head.t == INTEGER) {
            struct IntImage *int_img = (struct IntImage*)name->image;
            if (int_img->state != I) {
                all_instantiated = false;
                break;
            }
        }
    }
    
    if (all_instantiated) {
        // Create a linked list of solutions, one Instance per node
        for (uint i = 0; i < array->c_data.observed_count; i++) {
            array_linked_t *solution_tuple = laure_alloc(sizeof(array_linked_t));
            solution_tuple->data = array->c_data.observed_vars[i];
            solution_tuple->next = NULL;
            
            // Add this solution to the array's collection
            if (!array->c_data.solutions) {
                array->c_data.solutions = solution_tuple;
            } else {
                // Append to end of solution list
                array_linked_t *current = array->c_data.solutions;
                while (current->next) {
                    current = current->next;
                }
                current->next = solution_tuple;
            }
        }
        array->c_data.solution_count++;
    }
}

void laure_upd_scope(ulong link, laure_scope_t *to, laure_scope_t *from) {
    // unused
    Instance *to_ins = laure_scope_find_by_link(to, link, false);
    if (! to_ins) return;
    Instance *from_ins = laure_scope_find_by_link(from, link, false);
    if (! from_ins) return;
    bool res = image_equals(to_ins->image, from_ins->image, from);
    if (! res)
        printf("Error: cannot backtrack link %lu\n", link);
}

bool is_callable(void *image) {
    struct ImageHead head = read_head(image);
    return head.t == PREDICATE_FACT || head.t == CONSTRAINT_FACT;
}

size_t count_generic_place_idx(int name) {
    assert(name >= GENERIC_FIRST && name <= GENERIC_LAST);
    return name - GENERIC_FIRST;
}

bool is_weighted_expression(laure_expression_t *exp) {
    if (exp->t == let_rename || exp->t == let_command || exp->t == let_cut || exp->t == let_data)
        return false;
    return true;
}

#define UNPACK_CCTX(cctx) \
        laure_scope_t *scope = cctx->scope; \
        var_process_kit *vpk = cctx->vpk; \
        qcontext *qctx = cctx->qctx;

struct img_rec_ctx {
    Instance *var;
    control_ctx *cctx;
    laure_expression_set *expset;
    gen_resp (*qr_process)(qresp, struct img_rec_ctx*);
    uint flag;
    unsigned long link;
};

struct img_rec_ctx *img_rec_ctx_create(
    Instance *var, 
    control_ctx *cctx, 
    laure_expression_set *expset, 
    gen_resp (*qr_process)(qresp, struct img_rec_ctx*),
    unsigned long link
) {
    struct img_rec_ctx *ctx = laure_alloc(sizeof(struct img_rec_ctx));
    if (!ctx) return NULL;  /* Handle allocation failure */
    
    ctx->var = var;
    ctx->cctx = cctx;
    ctx->expset = expset;
    ctx->qr_process = qr_process;
    ctx->flag = 0;
    ctx->link = link;
    return ctx;
}

gen_resp image_rec_default(void *image, struct img_rec_ctx *ctx) {
    void *d = ctx->var->image;
    IMTYPE(read_head(ctx->var->image).t, ctx->var->image, image);
    laure_scope_t *nscope = laure_scope_create_copy(ctx->cctx, ctx->cctx->scope);
    laure_scope_t *oscope = ctx->cctx->scope;
    ctx->cctx->scope = nscope;
    qresp response = laure_start(ctx->cctx, ctx->expset);
    laure_scope_free(nscope);
    ctx->cctx->scope = oscope;
    IMTYPE(read_head(ctx->var->image).t, ctx->var->image, d);
    if (ctx->cctx->this_break) {
        gen_resp gr;
        gr.r = 0;
        gr.qr = respond(q_yield, (void*)0);
        return gr;
    }
    return ctx->qr_process(response, ctx);
}

gen_resp proc_unify_response(qresp resp, struct img_rec_ctx *ctx) {
    if (resp.state == q_stop || resp.state == q_error) {
        gen_resp gr = {0, resp};
        return gr;
    } else if (resp.state != q_true) {
        if (resp.state == q_yield) {
            if (resp.payload == YIELD_OK) {
                ctx->flag = 1;
            }
        }
        else if (resp.payload || ctx->cctx->no_ambig) {
            gen_resp gr = {0, respond(q_yield, 0)};
            return gr;
        }
    } else if (ctx->cctx->cut) {
        gen_resp gr = {0, respond(q_yield, 0)};
        return gr;
    } else if (resp.state == q_true) {
        ctx->flag = 1;
    }
    gen_resp gr = {1, resp};
    return gr;
}

gen_resp qr_process_default(qresp response, struct img_rec_ctx* ctx) {
    bool cont = true;
    if (
        (response.payload
        || response.state == q_stop)
        && response.state != q_yield
    ) {
        cont = false;
    } else if (response.state == q_yield && response.payload == YIELD_FAIL) {
        // Stop on YIELD_FAIL to prevent infinite loops in validation
        cont = false;
    } else if (ctx->cctx->cut) {
        cont = false;
    }
    gen_resp GR = {cont, response};
    return GR;
}

#define accuracy_frame(acc) acc >= 0.51 ? (acc == 1 ? GREEN_COLOR : YELLOW_COLOR) : RED_COLOR, acc, NO_COLOR

// Start evaluating search tree
qresp laure_start(control_ctx *cctx, laure_expression_set *expset) {
    LAST_QCTX = cctx->qctx;

    laure_expression_t *exp;
    qcontext *qctx = cctx->qctx;
    size_t sz = 0;
    #ifndef DISABLE_WS
    if (LAURE_WS) {
        sz = laure_count_transistions(cctx->ws);
    }
    #endif
    EXPSET_ITER(expset, exp, {
        laure_backtrace_add(LAURE_BACKTRACE, exp);
        qresp response = laure_eval(cctx, exp, _set->next);
        if (
            response.state == q_yield 
            || response.state == q_error 
            || response.state == q_stop
        ) {
            return response;
        } else if (response.state == q_false) {
            #ifndef DISABLE_WS
            if (LAURE_WS && is_weighted_expression(exp)) {
                if (laure_count_transistions(cctx->ws) == sz) {
                    // decision accuracy was not set
                    laure_push_transistion(cctx->ws, 0);
                }
            } else
            #endif
            return respond(q_yield, YIELD_FAIL);
        } else if (response.state == q_true) {
            #ifndef DISABLE_WS
            if (LAURE_WS && is_weighted_expression(exp)) {
                if (laure_count_transistions(cctx->ws) == sz) {
                    // decision accuracy was not set
                    laure_push_transistion(cctx->ws, 1);
                }
            }
            #endif
        }
        #ifndef DISABLE_WS
        if (LAURE_WS)
            sz = laure_count_transistions(cctx->ws);
        #endif
    });

    cctx->this_break = false;

    if (cctx->qctx)
        cctx->qctx->flagme = true;

    if (cctx->qctx && ((cctx->qctx->next && (cctx->scope->idx != 1)) || cctx->scope->repeat > 0)) {
        laure_scope_t *nscope;
        bool should_free = false;
        if (cctx->scope->repeat > 0) {
            nscope = cctx->scope;
            nscope->repeat--;
        } else if (cctx->scope->next) {
            should_free = true;
            nscope = laure_scope_create_copy(cctx, cctx->scope->next);
            laure_scope_iter(cctx->scope, cellptr, {
                Instance *sim = laure_scope_find_by_link(nscope, cellptr->link, false);
                if (sim) {
                    bool res = laure_safe_update(sim, cellptr->ptr, nscope, cctx->scope);
                    if (! res)
                        printf(
                            "Error: conflict when updating link %lu %s%s%s := %s%s%s\n", 
                            cellptr->link, 
                            RED_COLOR,
                            sim->repr(sim), 
                            NO_COLOR,
                            GREEN_COLOR,
                            cellptr->ptr->repr(cellptr->ptr),
                            NO_COLOR
                        );
                }
            });

        } else nscope = cctx->scope;
        qcontext *old = cctx->qctx;
        cctx->qctx = cctx->qctx ? cctx->qctx->next : NULL;
        cctx->scope = nscope;
        
        #ifndef DISABLE_WS
        // --- --- --- ---
        // Feature: weighted search
        // https://docs.laurelang.org/wiki/ws
        // --- --- --- ---
        laure_ws *current_ws, *ws_next;
        size_t sz_curr;
        if (LAURE_WS) {
            current_ws = cctx->ws;
            ws_next = laure_ws_next(current_ws);
            sz_curr = laure_count_transistions(current_ws);
            sz = laure_count_transistions(ws_next);
            
            optimality_t a = laure_optimality_count(current_ws);
            laure_push_transistion(ws_next, a);
            cctx->ws = ws_next;
        }
        #endif

        qresp resp = laure_start(
            cctx, 
            cctx->qctx ? cctx->qctx->expset : NULL
        );

        LAST_QCTX = old;

        #ifndef DISABLE_WS
        // detach transistions
        if (LAURE_WS) {
            laure_restore_transistions(ws_next, sz);
            laure_restore_transistions(current_ws, sz_curr);
            cctx->ws = current_ws;
        }
        #endif

        return resp;
    }

    qresp response;
    if (! cctx->silent && cctx->vpk && cctx->vpk->do_process) {
        debug("sending data (mode=%s%s%s)\n", BOLD_WHITE, cctx->vpk->mode == INTERACTIVE ? "INTERACTIVE" : (cctx->vpk->mode == SENDER_REPRS ? "SENDER_REPRS" : "OTHER"), NO_COLOR);

        #ifndef DISABLE_WS
        if (LAURE_WS) {
            optimality_t a = laure_optimality_count(cctx->ws);
            a = laure_ws_soften(a);
            string opt = get_dflag(WMIN_NAME);
            if (opt) {
                double dopt = atof(opt);
                if (a < dopt) {
                    return respond(q_yield, YIELD_OK);
                }
            }
            printf("\nOptimality = %s%f%s%s%s\n", accuracy_frame(a), GRAY_COLOR, NO_COLOR);
        }
        #endif
        if (cctx->vpk->mode == INTERACTIVE) {
            response = laure_showcast(cctx);
        } else if (cctx->vpk->mode == SENDER_REPRS) {
            response = laure_send(cctx);
        } else if (cctx->vpk->mode == SENDSCOPE) {
            cctx->vpk->scope_receiver(cctx, cctx->vpk->payload);
            response = respond(q_continue, NULL);
        }
    } else {
        response = respond(q_yield, YIELD_OK);
    }
    return response;
}

#define up printf("\033[A") 
#define down printf("\n") 
#define erase printf("\33[2K\r")

void laure_init_name_buffs() {
    if (IS_BUFFN_INITTED) return;
    RESPN = strdup("$R");
    CONTN = strdup("$C");
    MARKER_NODELETE = strdup("$NODELETE");
    DUMMY_FLAGV = strdup("true");
    for (uint idx = 0; idx < LAURE_PREDICATE_ARGC_MAX; idx++) {
        char name[4];
        snprintf(name, 4, "$%u", idx);
        ARGN_BUFF[idx] = strdup(name);
    }
    IS_BUFFN_INITTED = true;
}

string laure_get_argn(uint idx) {
    assert(IS_BUFFN_INITTED);
    assert(idx < LAURE_PREDICATE_ARGC_MAX);
    return ARGN_BUFF[idx];
}

string laure_get_respn() {
    assert(IS_BUFFN_INITTED);
    return RESPN;
}

string laure_get_contn() {
    assert(IS_BUFFN_INITTED);
    return CONTN;
}

void *crop_showcast(string showcast) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if (strlen(showcast) > w.ws_col) {
        showcast = showcast + w.ws_col;
    }
    return showcast;
}

qresp check_interactive(string cmd, string showcast) {
    if (cmd && str_eq(cmd, ".")) {
        // User claims no more solutions - validate completeness
        return respond(q_continue, (void*)9);  // Special validation marker
    }
    else if (! cmd || strlen(cmd) == 0) {
        up;
        erase;
        string ptr = showcast;
        showcast = crop_showcast(showcast);
        printf("%s;\n", showcast);
        if (strlen(ptr))
            laure_free(ptr);
        return respond(q_continue, NULL);
    }
    else if (str_eq(cmd, ";")) {
        return respond(q_continue, NULL);
    }
    else if (str_eq(cmd, "...")) {
        longjmp(JUMPBUF, 1);
        return respond(q_stop, (void*)1);
    }
    else {
        up;
        erase;
        showcast = crop_showcast(showcast);
        cmd = readline(showcast);
        return check_interactive(cmd, showcast);
    }
}

qresp laure_send(control_ctx *cctx) {
    laure_scope_t *scope = cctx->scope;
    var_process_kit *vpk = cctx->vpk;
    #ifdef LAURE_SENDER_USE_JSON_SENDER
    printf("not implemented json sender\n");
    exit(-1);
    #else
    if (vpk->tracked_vars_len == 0) return RESPOND_YIELD(YIELD_OK);
    char reprs[1024] = {0};
    strcpy(reprs, "=");
    for (int i = 0; i < vpk->tracked_vars_len; i++) {
        ulong link[1];
        Instance *glob = laure_scope_find_by_key_l(cctx->tmp_answer_scope, vpk->tracked_vars[i], link, false);
        if (! glob) continue;
        string repr;
        Instance *tracked = laure_scope_find_by_link(scope, *link, false);
        if (!tracked) repr = strdup("UNKNOWN");
        else {
            repr = instance_repr(tracked, scope);
        }
        strcat(reprs, repr);
        laure_free(repr);
        if (i != vpk->tracked_vars_len - 1) strcat(reprs, ";");
    }
    bool result = vpk->sender_receiver(strdup(reprs), vpk->payload);
    if (result) {
        return RESPOND_YIELD(YIELD_OK);
    } else {
        return respond(q_stop, 0);
    }
    #endif
}

qresp laure_showcast(control_ctx *cctx) {
    UNPACK_CCTX(cctx);

    string last_string = NULL;
    int j = 0;

    if (! vpk) return respond(q_true, 0);

    for (int i = 0; i < vpk->tracked_vars_len; i++) {
        ulong link[1];
        Instance *glob_ins = laure_scope_find_by_key_l(cctx->tmp_answer_scope, vpk->tracked_vars[i], link, false);
        if (! glob_ins) continue;
        Instance *ins = laure_scope_find_by_link(scope, *link, false);
        if (!ins) continue;

        glob_ins->image = image_deepcopy(ins->image);
        
        char name[64];
        strncpy(name, vpk->tracked_vars[i], 64);

        if (str_starts(name, "$")) {
            snprintf(name, 64, "%s", ins->name);
        }
        string repr = instance_repr(ins, scope);
        string showcast;

        if (strlen(repr) > 264) {
            printf("  %s = ", name);
            printf("%s\n", repr);
            showcast = "";
        } else {
            uint showcast_n = strlen(name) + strlen(repr) + 6;
            showcast = laure_alloc(showcast_n + 1);
            memset(showcast, 0, showcast_n + 1);
            snprintf(showcast, showcast_n, "  %s = %s", name, repr);
        }

        laure_free(repr);

        last_string = showcast;

        if (i + 1 != vpk->tracked_vars_len) {
            printf("%s,\n", showcast);
        } else {
            string cmd = readline(showcast);
            qresp interactive_resp = check_interactive(cmd, showcast);
            
            // Handle completeness validation request
            
            return interactive_resp;
        }
        j++;
    }

    if (j != vpk->tracked_vars_len) {
        if (last_string != NULL) {
            up;
            erase;
            string cmd = readline(last_string);
            qresp interactive_resp = check_interactive(cmd, last_string);
            
            
            return interactive_resp;
        }
    }

    return RESPOND_YIELD(YIELD_OK);
}

/* ===================
| Var processing kit |
=================== */

void default_var_processor(laure_scope_t *scope, string name, void* _) {
    Instance *ins = laure_scope_find_by_key(scope, name, true);
    string s = instance_repr(ins, scope);
    printf("  %s\n", s);
    laure_free(s);
}

int append_vars(string **vars, int vars_len, laure_expression_set *vars_exps) {
    laure_expression_t *exp = NULL;
    EXPSET_ITER(vars_exps, exp, {
        string n = exp->s;
        assert(n);
        if (str_eq(n, ANONVAR_NAME)) {}
        else {
            bool exists = false;
            for (int i = 0; i < vars_len; i++) {
                if (strcmp((*vars)[i], n) == 0) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                (*vars) = laure_realloc(*vars, sizeof(void*) * (vars_len + 1));
                (*vars)[vars_len] = n;
                vars_len++;
            }
        }
    });
    return vars_len;
}

var_process_kit *laure_vpk_create(laure_expression_set *expset) {
    MEMGUARD_CLEANUP_CONTEXT(cleanup);
    
    string *vars = MEMGUARD_ALLOC(&cleanup, sizeof(string));
    if (!vars) {
        MEMGUARD_CLEANUP(&cleanup);
        return NULL;
    }
    int vars_len = 0;

    laure_expression_t *exp = NULL;
    EXPSET_ITER(expset, exp, {
        laure_expression_set *vars_exps = laure_get_all_vars_in(exp, NULL);
        vars_len = append_vars(&vars, vars_len, vars_exps);
    });

    var_process_kit *vpk = MEMGUARD_ALLOC(&cleanup, sizeof(var_process_kit));
    if (!vpk) {
        MEMGUARD_CLEANUP(&cleanup);
        return NULL;
    }
    
    vpk->do_process = true;
    vpk->interactive_by_name = false;
    vpk->mode = INTERACTIVE;
    vpk->payload = NULL;
    vpk->single_var_processor = default_var_processor;
    vpk->tracked_vars = vars;
    vpk->tracked_vars_len = vars_len;
    
    /* Transfer ownership - vpk now owns vars and itself */
    MEMGUARD_TRANSFER_OWNERSHIP(&cleanup);
    MEMGUARD_CLEANUP(&cleanup);
    return vpk;
}

void laure_vpk_free(var_process_kit *vpk) {
    laure_free(vpk->tracked_vars);
    laure_free(vpk);
}

#define _laure_eval_sub_args control_ctx *cctx, laure_expression_t *e, laure_expression_set *expset
qresp laure_eval_assert(_laure_eval_sub_args);
qresp laure_eval_image(_laure_eval_sub_args);
qresp laure_eval_rename(_laure_eval_sub_args);
qresp laure_eval_name(_laure_eval_sub_args);
qresp laure_eval_unify(_laure_eval_sub_args);
qresp laure_eval_pred_call(_laure_eval_sub_args);
qresp laure_eval_callable_decl(_laure_eval_sub_args);
qresp laure_eval_quant(_laure_eval_sub_args);
qresp laure_eval_imply(_laure_eval_sub_args);
qresp laure_eval_choice(_laure_eval_sub_args);
qresp laure_eval_cut(_laure_eval_sub_args);
qresp laure_eval_set(_laure_eval_sub_args);
qresp laure_eval_atom_sign(_laure_eval_sub_args);
qresp laure_eval_command(_laure_eval_sub_args);
qresp laure_eval_structure(_laure_eval_sub_args);
qresp laure_eval_array(_laure_eval_sub_args);

qresp laure_eval(control_ctx *cctx, laure_expression_t *e, laure_expression_set *expset) {
    laure_scope_t *scope = cctx->scope;
    var_process_kit *vpk = cctx->vpk;
    qcontext *qctx = cctx->qctx;

    debug("evaluating expression %s\n", EXPT_NAMES[e->t]);

    switch (e->t) {
        case let_assert: {
            return laure_eval_assert(cctx, e, expset);
        }
        case let_image: {
            return laure_eval_image(cctx, e, expset);
        }
        case let_rename: {
            return laure_eval_rename(cctx, e, expset);
        }
        #ifndef FORBID_NAME_EVAL
        case let_name: {
            return laure_eval_name(cctx, e, expset);
        }
        #endif
        #ifndef FORBID_FORCEUNIFY
        case let_unify: {
            return laure_eval_unify(cctx, e, expset);
        }
        #endif
        case let_pred_call: {
            return laure_eval_pred_call(cctx, e, expset);
        }
        #ifndef FORBID_QUANTORS
        case let_quant: {
            return laure_eval_quant(cctx, e, expset);
        }
        #endif
        case let_imply: {
            return laure_eval_imply(cctx, e, expset);
        }
        case let_pred:
        case let_constraint: {
            return laure_eval_callable_decl(cctx, e, expset);
        }
        case let_choice_2: {
            return laure_eval_choice(cctx, e, expset);
        }
        case let_cut: {
            return laure_eval_cut(cctx, e, expset);
        }
        case let_set: {
            return laure_eval_set(cctx, e, expset);
        }
        case let_atom_sign: {
            return laure_eval_atom_sign(cctx, e, expset);
        }
        #ifndef FORBID_COMMAND
        case let_command: {
            return laure_eval_command(cctx, e, expset);
        }
        #endif
        case let_struct: {
            return laure_eval_structure(cctx, e, expset);
        }
        case let_array: {
            return laure_eval_array(cctx, e, expset);
        }
        default: {
            RESPOND_ERROR(internal_err, e, "can't evaluate `%s` in this context", EXPT_NAMES[e->t]);
        }
    }
}

qresp gen_resp_process(gen_resp gr) {
    if (gr.qr.payload)
        return gr.qr;
    return RESPOND_YIELD(YIELD_OK);
}

/* =-------=
Assertion.
* Guarantees asserted instances
to get instantiated. May create a
choicepoint.
=-------= */

qresp laure_eval_assert(
    control_ctx *cctx, 
    laure_expression_t *e,
    laure_expression_set *expset
) {
    assert(e->t == let_assert);

    UNPACK_CCTX(cctx);

    laure_expression_t *lvar = laure_expression_set_get_by_idx(e->ba->set, 0);
    laure_expression_t *rvar = laure_expression_set_get_by_idx(e->ba->set, 1);
    
    if (lvar->t == let_name && rvar->t == let_name) {
        // When var asserted to var
        // check whether they exist or not

        ulong l1[1], l2[1];
        Instance *lvar_ins = laure_scope_find_by_key_l(scope, lvar->s, l1, true);
        Instance *rvar_ins = laure_scope_find_by_key_l(scope, rvar->s, l2, true);

        if (lvar_ins && rvar_ins) {
            // check if they are instantiated
            bool li = instantiated(lvar_ins);
            bool ri = instantiated(rvar_ins);

            if (li && ri) {
                return image_equals(lvar_ins->image, rvar_ins->image, scope) ? RESPOND_TRUE : RESPOND_FALSE;
            } else if (li || ri) {
                Instance *from;
                Instance *to;

                if (li) {
                    from = lvar_ins;
                    to = rvar_ins;
                } else {
                    from = rvar_ins;
                    to = lvar_ins;
                }

                if (to->locked) {
                    RESPOND_ERROR(access_err, e, "%s is locked", to->name);
                }

                struct ImageHead h = read_head(to->image);
                struct ImageHead oldh = read_head(from->image);

                return image_equals(from->image, to->image, scope) ? RESPOND_TRUE : RESPOND_FALSE;
            } else {
                // choicepoint is created
                // due to ambiguation
                // **intersection**
                Instance *var1;
                Instance *var2;
                unsigned long v2_link;
                if (lvar_ins->locked && !rvar_ins->locked) {
                    var1 = lvar_ins;
                    var2 = rvar_ins;
                    v2_link = *l2;
                } else if (!lvar_ins->locked || !rvar_ins->locked) {
                    var1 = rvar_ins;
                    var2 = lvar_ins;
                    v2_link = *l1;
                } else {
                    RESPOND_ERROR(
                        access_err,
                        e,
                        "both %s and %s are locked variables, so no ambiguation may be created", 
                        lvar_ins->name, rvar_ins->name
                    );
                }

                bool eqr = image_equals(var1->image, var2->image, scope);
                if (! eqr) {
                    return RESPOND_FALSE;
                }

                // context
                struct img_rec_ctx *ctx = img_rec_ctx_create(var2, cctx, expset, qr_process_default, v2_link);
                // -------
                gen_resp gr = instance_generate(scope, var1, image_rec_default, ctx);
                laure_free(ctx);

                if (gr.r == 0) {
                    if (gr.qr.state == q_error) return gr.qr;
                }

                return gen_resp_process(gr);
            }
        } else if (lvar_ins || rvar_ins) {
            string nvar_name = rvar->s;
            Instance *srcvar = lvar_ins;

            if (rvar_ins) {
                srcvar = rvar_ins;
                nvar_name = lvar->s;
            }

            Instance *nvar = instance_deepcopy(nvar_name, srcvar);
            nvar->locked = false;

            laure_scope_insert(scope, nvar);

            return RESPOND_TRUE;
        } else {
            RESPOND_ERROR(undefined_err, e, "both variables %s and %s are undefined and cannot be asserted", lvar->s, rvar->s);
        }
    } else if (lvar->t == let_name || rvar->t == let_name) {
        string vname = lvar->t == let_name ? lvar->s : rvar->s;
        laure_expression_t *rexpression = lvar->t == let_name ? rvar : lvar;
        ulong link;
        Instance *to = laure_scope_find_by_key_l(scope, vname, &link, true);

        if (!to)
            RESPOND_ERROR(undefined_err, e, "name %s", vname);
        
        if (to->locked)
            RESPOND_ERROR(access_err, e, "%s is locked", vname);
        else {
            // Expression is sent to image translator
            struct ImageHead head = read_head(to->image);

            if (head.t == PREDICATE_FACT) {
                struct PredicateImage *pim = (struct PredicateImage*)to->image;
                if (! pim->header.resp || pim->header.resp->t != td_auto)
                    RESPOND_ERROR(signature_err, e, "predicate response argument is not auto");
            }

            if (! head.translator)
                RESPOND_ERROR(internal_err, e, "%s is not assignable. Cannot assign to `%s`", vname, rexpression->s);
            
            bool result = head.translator->invoke(rexpression, to->image, scope, link);
            if (!result) return RESPOND_FALSE;

            if (head.t == PREDICATE_FACT) {
                laure_uuid_image *pim = (laure_uuid_image*)to->image;
                to->repr = uuid_repr;
            }
            return RESPOND_TRUE;
        }
    } else
        RESPOND_ERROR(instance_err, e, "can't assert %s with %s", lvar->s, rvar->s);
}

// generates array of fixed size
// array contains blueprint atom elements
Instance *get_nested_fixed(Instance *atom, uint length, laure_scope_t *scope) {
    struct ArrayImage *im = laure_create_array_u(atom);
    laure_free(im->u_data.length);
    im->state = I;
    im->i_data.length = length;
    array_linked_t *first = NULL, *linked = NULL;
    for (uint i = 0; i < length; i++) {
        array_linked_t *new = laure_alloc(sizeof(array_linked_t));
        new->data = instance_deepcopy(MOCK_NAME, atom);
        new->next = linked;
        if (linked)
            linked->next = new;
        linked = new;
        if (! first)
            first = linked;
    }
    im->i_data.linked = first;
    Instance *ins = instance_new(MOCK_NAME, NULL, im);
    ins->repr = array_repr;
    return ins;
}

Instance *ready_instance(laure_scope_t *scope, laure_expression_t *expr) {
    if (expr->t == let_name) {
        Instance *ins = laure_scope_find_by_key(scope, expr->s, true);
        if (! ins) return NULL;
        return instance_deepcopy(ins->name, ins);
    } else if (expr->t == let_nested) {
        Instance *atom = ready_instance(scope, expr->link);
        if (! atom) return NULL;
        return get_nested_instance(atom, expr->flag, scope);
    } else if (expr->t == let_pred_call) {
        if (str_eq(expr->s, "by_idx")) {
            laure_expression_t *typevar_expr = expr->ba->set->expression;
            laure_expression_t *idx_expr = expr->ba->set->next->expression;
            Instance *typevar = ready_instance(scope, typevar_expr);

            if (! typevar) return NULL;
            switch (idx_expr->t) {
                case let_name: {
                    unsigned long link[1];
                    Instance *ins = laure_scope_find_by_key_l(scope, idx_expr->s, link, true);
                    if (! ins) {
                        ins = instance_new(idx_expr->s, NULL, laure_create_integer_u(
                            int_domain_new()
                        ));
                        ins->repr = int_repr;
                        *link = laure_scope_generate_link();
                        laure_scope_insert_l(scope, ins, *link);
                    } else {
                        //! todo handle locked
                        if (read_head(ins->image).t != INTEGER) {
                            return NULL;
                        }
                        if (instantiated(ins)) {
                            uint length = (uint)bigint_double(((struct IntImage*)(ins->image))->i_data);
                            return get_nested_fixed(typevar, length, scope);
                        }
                    }
                    Instance *nested = get_nested_instance(typevar, 1, scope);
                    assert(nested);
                    struct ArrayImage *ary = (struct ArrayImage*)nested->image;
                    ary->length_lid = *link;
                    return nested;
                }
                case let_data: {
                    uint length = (uint)atoi(idx_expr->s);
                    Instance *nested = get_nested_fixed(typevar, length, scope);
                    return nested;
                }
                default: {
                    image_free(typevar->image);
                    laure_free(typevar);
                    return NULL;
                }
            }
        }
    }
    return NULL;
}

bool is_predicate_image(void *img) {
    int t = IMAGET(img);
    return t == PREDICATE_FACT || t == CONSTRAINT_FACT;
}

laure_union_image *create_union_from_clarifiers(laure_scope_t *scope, laure_expression_set *expr_set) {
    laure_expression_t *A_expr = expr_set->expression;
    assert(A_expr->t == let_name);

    Instance *A = laure_scope_find_by_key(scope, A_expr->s, true);
    if (! A)
        return NULL;
    
    A = instance_deepcopy(A->name, A);
    Instance *B = NULL;

    if (expr_set->next->next) {
        laure_union_image *next_union = create_union_from_clarifiers(scope, expr_set->next);
        if (! next_union) {
            image_free(A->image);
            laure_free(A);
            return NULL;
        }
        B = instance_new(MOCK_NAME, NULL, next_union);
        B->repr = union_repr;
    } else {
        assert(expr_set->next->expression->t == let_name);
        B = laure_scope_find_by_key(scope, expr_set->next->expression->s, true);
        if (! B) {
            image_free(A->image);
            laure_free(A);
            return NULL;
        }
        B = instance_deepcopy(B->name, B);
    }
    return laure_union_create(A, B);
}

/* Memory-safe version of create_union_from_clarifiers using memguard */
laure_union_image *create_union_from_clarifiers_safe(laure_scope_t *scope, laure_expression_set *expr_set) {
    cleanup_ctx_t cleanup;
    cleanup_ctx_init(&cleanup);
    
    laure_expression_t *A_expr = expr_set->expression;
    assert(A_expr->t == let_name);

    Instance *A = laure_scope_find_by_key(scope, A_expr->s, true);
    if (!A) return NULL;
    
    A = instance_deepcopy(A->name, A);
    cleanup_register_instance(&cleanup, A);  /* Auto-cleanup on error */
    
    Instance *B = NULL;

    if (expr_set->next->next) {
        laure_union_image *next_union = create_union_from_clarifiers_safe(scope, expr_set->next);
        if (!next_union) {
            return NULL;  /* cleanup context handles A automatically */
        }
        
        B = instance_new(MOCK_NAME, NULL, next_union);
        if (!B) return NULL;
        B->repr = union_repr;
        cleanup_register_instance(&cleanup, B);
        
    } else {
        assert(expr_set->next->expression->t == let_name);
        B = laure_scope_find_by_key(scope, expr_set->next->expression->s, true);
        if (!B) {
            return NULL;  /* cleanup context handles A automatically */
        }
        B = instance_deepcopy(B->name, B);
        cleanup_register_instance(&cleanup, B);
    }
    
    laure_union_image *result = laure_union_create(A, B);
    if (result) {
        /* Success - transfer ownership to result, remove from cleanup */
        cleanup.count = 0;  /* Prevent auto-cleanup since ownership transferred */
    } else {
        /* Failure - cleanup will handle A and B */
        cleanup_ctx_destroy(&cleanup);
        return NULL;
    }
    
    cleanup_ctx_destroy(&cleanup);  /* Clean up context but not registered objects */
    return result;
}

/* =-------=
Imaging.
* Never creates a choicepoint
* Creates atomic sets:
  `S ~ @{a, b, c};`
* Initializes nested instances
  `I ~ int[]`
=-------= */

qresp laure_eval_image(
    control_ctx *cctx, 
    laure_expression_t *e,
    laure_expression_set *expset
) {
    assert(e->t == let_image);

    UNPACK_CCTX(cctx);

    laure_expression_t *exp1 = laure_expression_set_get_by_idx(e->ba->set, 0);
    laure_expression_t *exp2 = laure_expression_set_get_by_idx(e->ba->set, 1);

    if (exp2->t == let_atom && exp2->flag == 1) {
        // Feature: atom universum declaration
        if (exp1->t != let_name) RESPOND_ERROR(syntaxic_err, e, "left value imaged to atomic set %s should be a variable", exp2->s);
        Instance *ins = laure_scope_find_by_key(scope, exp1->s, true);
        if (ins) {
            if (read_head(ins->image).t != ATOM) return RESPOND_FALSE;
        }
        multiplicity *mult = multiplicity_create();
        laure_expression_t *ptr = NULL;
        EXPSET_ITER(exp2->ba->set, ptr, {
            char name[ATOM_LEN_MAX];
            write_atom_name(ptr->s, name);
            string cpname = strdup(name);
            if (multiplicity_insert(mult, cpname, strcmp) != 0) {
                free(cpname);
            }
        });
        struct AtomImage *atom = laure_atom_universum_create(mult);
        if (! ins) {
            // create new
            ins = instance_new(exp1->s, NULL, atom);
            ins->repr = atom_repr;
            laure_scope_insert(scope, ins);
        } else {
            // atom reassignation (in doubt)
            image_free(ins->image);
            ins->image = atom;
        }
        return RESPOND_TRUE;
    }

    if (exp1->t == let_pred || exp2->t == let_pred) {
        if (exp1->t == let_pred && exp2->t == let_pred)
            RESPOND_ERROR(syntaxic_err, e, "invalid image Predicate to Predicate");
        laure_expression_t *to, *pred;
        if (exp1->t == let_pred) {
            pred = exp1;
            to = exp2;
        } else {
            pred = exp2;
            to = exp1;
        }
        if (! PREDFLAG_IS_PRIMITIVE(pred->flag))
            RESPOND_ERROR(syntaxic_err, e, "must be primitive (unnamed predicate signature)");
        
        if (to->t != let_name)
            RESPOND_ERROR(syntaxic_err, to, "must be variable");

        Instance *ins = laure_scope_find_by_key(scope, to->s, true);
        if (ins) return RESPOND_FALSE;
        
        struct PredicateImage *predicate = (struct PredicateImage*) laure_apply_pred(pred, scope);

        if (! predicate)
            RESPOND_ERROR(internal_err, e, "cannot apply predicate");

        predicate->is_primitive = true;
        
        ins = instance_new(to->s, NULL, predicate);
        ins->repr = predicate_repr;
        laure_scope_insert(scope, ins);
        return RESPOND_TRUE;
    }

    if (exp1->t == let_name && exp2->t == let_name) {

        Instance *var1 = laure_scope_find_var(scope, exp1, true);
        Instance *var2 = laure_scope_find_var(scope, exp2, true);

        if (var1 && var2)  {
            // Check variables derivation
            void *image_1  = var1->image;
            void *image_2  = var2->image;

            // Nesting unpack
            
            uint nesting_1 =  exp1->flag;
            uint nesting_2 =  exp2->flag;

            absorb(nesting_1, nesting_2);
            assert(! (nesting_1 && nesting_2));

            while (nesting_1 || nesting_2) {
                if (nesting_1) {
                    if (read_head(image_2).t != ARRAY) return RESPOND_FALSE;
                    image_2 = ((struct ArrayImage*) image_2)->arr_el->image;
                    nesting_1--;
                }
                if (nesting_2) {
                    if (read_head(image_1).t != ARRAY) return RESPOND_FALSE;
                    image_1 = ((struct ArrayImage*) image_1)->arr_el->image;
                    nesting_2--;
                }
            }
            
            while (read_head(image_1).t == ARRAY && read_head(image_2).t == ARRAY) {
                image_1 = ((struct ArrayImage*) image_1)->arr_el->image;
                image_2 = ((struct ArrayImage*) image_2)->arr_el->image;
            }
            if (nesting_1 != nesting_2) return RESPOND_FALSE;
            if (var1 == var2) return RESPOND_TRUE;
            
            return read_head(image_1).t == read_head(image_2).t ? RESPOND_TRUE : RESPOND_FALSE;

        } else if (var1 || var2) {
            Instance *from;
            laure_expression_t *new_var, *old_var;
            uint nesting;
            uint secondary_nesting;

            if (var1) {
                from = var1;
                new_var = exp2;
                old_var = exp1;
                nesting = exp1->flag;
                secondary_nesting = exp2->flag;
            } else {
                from = var2;
                new_var = exp1;
                old_var = exp2;
                nesting = exp2->flag;
                secondary_nesting = exp1->flag;
            }

            if (old_var->ba)
                if (IMAGET(from->image) != PREDICATE_FACT && IMAGET(from->image) != CONSTRAINT_FACT)
                    RESPOND_ERROR(instance_err, e, "type can only be bound to predicate");

            absorb(nesting, secondary_nesting);

            Instance *ins = instance_deepcopy(new_var->s, from);

            if (IMAGET(ins->image) == STRUCTURE) {
                qresp r = structure_init(ins->image, scope);
                if (r.state != q_true) {
                    image_free(ins->image);
                    laure_free(ins);
                    return r;
                }
            }

            if (old_var->ba) {
                if (! is_predicate_image(ins->image)) {
                    RESPOND_ERROR(not_implemented_err, e, "cannot apply type clarification on non predicate image (got %s)", IMG_NAMES[IMAGET(ins->image)]);
                } else if (((struct PredicateImage*)ins->image)->variations->set->t == PREDICATE_C) {
                    if (true) {
                        if (old_var->ba->body_len < 2) {
                            RESPOND_ERROR(syntaxic_err, e, "union elements must be greater than one");
                        }
                        laure_expression_t *ptr;
                        EXPSET_ITER(old_var->ba->set, ptr, {
                            if (ptr->t != let_name) {
                                RESPOND_ERROR(syntaxic_err, e, "all union clarifiers must be names");
                            }
                        });
                        laure_union_image *image = create_union_from_clarifiers(scope, old_var->ba->set);

                        if (!image) {
                            RESPOND_ERROR(instance_err, e, "union must be concluded on already bound instances");
                        }
                        ins->repr = union_repr;
                        laure_free(ins->image);
                        ins->image = image;

                    } else {
                        RESPOND_ERROR(instance_err, e, "builtin callable does not provide clarifier application");
                    }
                } else {
                    predicate_bound_types_result pbtr = laure_dom_predicate_bound_types(scope, ins->image, old_var->ba->set);
                    if (pbtr.code != 0) {
                        RESPOND_ERROR(internal_err, e, "cannot bound types; %s, code=%d", pbtr.err, pbtr.code);
                    } else {
                        laure_free(ins->image);
                        ins->image = pbtr.bound_predicate;
                    }
                }
            }

            if (nesting) {
                while (nesting) {
                    void *img = laure_create_array_u(ins);
                    ins = instance_new(MOCK_NAME, NULL, img);
                    ins->repr = array_repr;
                    nesting--;
                }
                if (! str_eq(new_var->s, "_")) {
                    ins->name = strdup(new_var->s);
                }
                ins->repr = array_repr;
            }

            if (secondary_nesting > 0) {
                Instance *unwrapped = laure_unwrap_nestings(ins, secondary_nesting);
                if (! unwrapped) {
                    return RESPOND_FALSE;
                }
                ins->image = unwrapped->image;
                ins->repr = unwrapped->repr;
            }
            if (! str_eq(new_var->s, "_")) {
                laure_scope_insert(scope, ins);
            }
            return RESPOND_TRUE;
        } else
            RESPOND_ERROR(undefined_err, e, "both variables %s and %s are unknown", exp1->s, exp2->s);
    } else if (exp1->t == let_name && (exp2->t == let_pred_call || exp2->t == let_nested)) {
        // valid nested:
        // `arr ~ int[i][]
        // valid predcall:
        // `arr ~ by_idx(int[], i)`
        // `arr ~ int[][i]`
        string vname = exp1->s;
        Instance *ins = ready_instance(scope, exp2);
        if (! ins)
            RESPOND_ERROR(internal_err, exp2, "invalid nested imaging");
        //if (laure_scope_find_by_key(scope, vname, true)) 
        //    RESPOND_ERROR(not_implemented_err, exp2, "imaging nested on known instance");
        ins->name = vname;
        laure_scope_insert(scope, ins);
        return RESPOND_TRUE;

    } else {
        RESPOND_ERROR(instance_err, e, "invalid imaging %s to %s", EXPT_NAMES[exp1->t], EXPT_NAMES[exp2->t]);
    }
}

/* =-------=
Naming.
Renames instance.
!!! Non backtracable operation, 
    should only be performed right
    after private scope initialization.
!!! Unsafe operation. Shouldn't be allowed
    in parsed AST. Non-symmetrical
=-------= */
qresp laure_eval_rename(_laure_eval_sub_args) {
    assert(e->t == let_rename);
    UNPACK_CCTX(cctx);
    laure_expression_t *v1_exp = e->ba->set->expression;
    laure_expression_t *v2_exp = e->ba->set->next->expression;

    ulong link1[1], link2[1];

    // the problem is:
    // when we say we only use local vars
    // we get used names in global scope
    // and for example `int[] arr = [1,2,3], append([], _) = arr`
    // wont work because append declares its own arr
    // otherwise when we prohibit them we get wrong answer on
    // `image(string) = x` because it uses global locked instances to
    // bind universum to atom
    Instance *v1 = laure_scope_find_by_key_l(scope, v1_exp->s, link1, false);
    Instance *v2 = laure_scope_find_by_key_l(scope, v2_exp->s, link2, false);

    if (v1 && ! v2) {
        v2 = laure_scope_find_by_key(scope->glob, v2_exp->s, false);
        if (v2 && ! v2->locked)
            v2 = NULL;
    }

    if (!v1 && !v2) {
        return RESPOND_TRUE;
    } else if (v1 && v2) {
        if (v1 == v2) return RESPOND_TRUE;
        return image_equals(v1->image, v2->image, scope) ? RESPOND_TRUE : RESPOND_FALSE;
        /*
        else if (instantiated(v1) || instantiated(v2)) {
            return image_equals(v1->image, v2->image, scope) ? RESPOND_TRUE : RESPOND_FALSE;
        } else {
            laure_scope_change_link_by_key(scope, v2_exp->s, link1[0], false);
            return RESPOND_TRUE;
        }*/
    } else {
        Instance *renamed = v1 ? v1 : v2;
        renamed->name = v1 ? v2_exp->s : v1_exp->s;
        renamed->doc = v2_exp->docstring;
        return RESPOND_TRUE;
    }
}

/* =-------=
Unify.
Forces variable unification.
Creates a choicepoint.
=-------= */
qresp laure_eval_unify(_laure_eval_sub_args) {
    assert(e->t == let_unify);
    UNPACK_CCTX(cctx);
    ulong link[1];
    Instance *to_unif = laure_scope_find_by_key_l(scope, e->s, link, false);
    if (! to_unif)
        RESPOND_ERROR(undefined_err, e, "name %s", e->s);
    else if (to_unif->locked)
        RESPOND_ERROR(access_err, e, "%s is locked", e->s);
    struct img_rec_ctx *ctx = img_rec_ctx_create(to_unif, cctx, expset, proc_unify_response, *link);
    gen_resp gr = instance_generate(scope, to_unif, image_rec_default, ctx);
    if (gr.qr.state == q_error || gr.qr.state == q_stop || ! gr.r) return gr.qr;
    if (! ctx->flag) return respond(q_yield, YIELD_FAIL);
    laure_free(ctx);
    if (gr.qr.state == q_stop) {
        return gr.qr;
    }
    return respond(q_yield, gr.qr.payload);
}

/* =-------=
Shows variable.
=-------= */
qresp laure_eval_name(_laure_eval_sub_args) {
    assert(e->t == let_name);
    UNPACK_CCTX(cctx);

    if (e->ba) {
        RESPOND_ERROR(internal_err, e, "invalid usage of template");
    }

    if (!vpk) return RESPOND_TRUE;

    string vname = e->s;
    if (e->flag > 0)
        RESPOND_ERROR(instance_err, e, "cannot evaluate nesting of %s, use infer op.", vname);
    
    Instance *var = laure_scope_find_by_key(scope, vname, true);
    if (!var) RESPOND_ERROR(undefined_err, e, "name %s", vname);

    vpk->single_var_processor(scope, vname, vpk->payload);
    // cctx->silent = true;
    return RESPOND_TRUE;
}

struct arg_rec_ctx {
    uint idx_pd;
    union {
        preddata *pd;
        laure_scope_t *new_scope;
    };
    laure_scope_t *prev_scope;
};

Instance *get_array_derived(struct ArrayImage *arr, laure_scope_t *scope) {
    // going down to primitive
    uint nesting = 1;
    Instance *arrel = arr->arr_el;
    while (read_head(arrel->image).t == ARRAY) {
        arrel = ((struct ArrayImage*)arrel->image)->arr_el;
        nesting++;
    }
    return get_nested_instance(arrel, nesting, scope);
}

Instance *get_derived_instance(laure_scope_t *scope, Instance *resolved_instance) {
    void *image = resolved_instance->image;
    laure_image_head head = read_head(image);
    if (! head.translator) {
        return resolved_instance;
    }
    if (head.translator->identificator == INT_TRANSLATOR->identificator) {
        return laure_scope_find_by_key(scope->glob, "int", false);
    } else if (head.translator->identificator == CHAR_TRANSLATOR->identificator) {
        return laure_scope_find_by_key(scope->glob, "char", false);
    } else if (head.translator->identificator == ARRAY_TRANSLATOR->identificator) {
        return get_array_derived(resolved_instance->image, scope);
    } else if (head.translator->identificator == ATOM_TRANSLATOR->identificator) {
        return resolved_instance;
    } else if (head.translator->identificator == STRING_TRANSLATOR->identificator) {
        return laure_scope_find_by_key(scope->glob, "string", false);
    } else if (head.translator->identificator == UNION_TRANSLATOR->identificator) {
        return resolved_instance;
    } else if (head.translator->identificator == STRUCTURE_TRANSLATOR->identificator) {
        return resolved_instance;
    }
    return NULL;
}

Instance *prepare_T_instance(laure_scope_t *scope, Instance *resolved, uint nesting) {
    Instance *prepared = laure_unwrap_nestings(resolved, nesting);
    if (! prepared) {
        debug("Unable to unwrap nestings to perform generic preparation\n");
        return NULL;
    }
    prepared = get_derived_instance(scope, prepared);
    if (! prepared) {
        debug("Unable to find what instance datatype was derived from; notice that atom datatype cannot be used in generic\n");
        return NULL;
    }
    prepared = instance_shallow_copy(prepared);
    return prepared;
}

Instance *resolve_via_signature(
    int name,
    Instance **Generics,
    struct PredicateHeaderImage header,
    Instance *signed_predicate,
    laure_scope_t *scope
) {
    assert(signed_predicate);
    struct PredicateImage *predicate = (struct PredicateImage*) signed_predicate->image;
    if (! predicate->header.args) return NULL;

    for (uint i = 0; i < header.args->length; i++) {
        laure_typedecl td = header.args->data[i];
        if (td.t == td_generic) {
            int n = td.generic;
            size_t idx = count_generic_place_idx(n);
            if (Generics[idx]) {
                // set generic to instance in predicate instance
                predicate->header.args->data[i].t = td_instance;
                predicate->header.args->data[i].instance = Generics[idx];
                continue;
            }
            if (predicate->header.args->data[i].t == td_instance) {
                Generics[idx] = prepare_T_instance(scope, predicate->header.args->data[i].instance, predicate->header.nestings[i]);
            }
        } else if (td.t == td_instance && IMAGET(td.instance->image) == PREDICATE_FACT) {
            Instance *ins = resolve_via_signature(name, Generics, ((struct PredicateImage*)td.instance->image)->header, predicate->header.args->data[i].instance, scope);
        }
    }
    if (header.resp) {
        laure_typedecl td = *header.resp;;
        if (td.t == td_generic) {
            int n = td.generic;
            size_t idx = count_generic_place_idx(n);
            if (! Generics[idx] && predicate->header.resp->t == td_instance) {
                Generics[idx] = prepare_T_instance(scope, predicate->header.resp->instance, predicate->header.response_nesting);
            } else if (Generics[idx]) {
                predicate->header.resp->t = td_instance;
                predicate->header.resp->instance = Generics[idx];
            }
        } else if (td.t == td_instance && IMAGET(td.instance->image) == PREDICATE_FACT) {
            Instance *ins = resolve_via_signature(name, Generics, ((struct PredicateImage*)td.instance->image)->header, predicate->header.resp->instance, scope);
        }
    }
    if (! name)
        return NULL;
    return Generics[count_generic_place_idx(name)];
}

Instance *resolve_generic_T(
    int name,
    Instance **Generics,
    struct PredicateHeaderImage header,
    laure_expression_set *set,
    laure_scope_t *scope,
    int *code
) {
    if (header.resp) {
        laure_expression_t *rexp = laure_expression_set_get_by_idx(set, header.args->length);
        if (rexp && rexp->t == let_name) {
            Instance *resolved = laure_scope_find_by_key(scope, rexp->s, false);
            if (header.resp->t == td_generic && header.resp->generic == name) {
                if (resolved) {
                    uint nesting = header.response_nesting;
                    debug("T resolved by response\n");
                    Instance *final =  prepare_T_instance(scope, resolved, nesting);
                    debug("Resolved instance: %s\n", final->repr(final));
                    *code = 1;
                    return final;
                }
            } else if (
                resolved
                && header.resp 
                && header.resp->t == td_instance 
                && is_callable(header.resp->instance->image)
            ) {
                *code = 0;
                return resolve_via_signature(name, Generics, ((struct PredicateImage*)header.resp->instance->image)->header, resolved, scope);
            }
        }
    }

    for (uint i = 0; i < header.args->length; i++) {
        laure_expression_t *exp = laure_expression_set_get_by_idx(set, i);
        if (exp->t == let_name) {
            Instance *resolved = laure_scope_find_by_key(scope, exp->s, true);
            laure_typedecl td = header.args->data[i];
            if (td.t == td_generic && td.generic == name) {
                if (resolved) {
                    uint nesting = header.nestings[i];
                    debug("T resolved by argument %u\n", i);
                    Instance *final = prepare_T_instance(scope, resolved, nesting);
                    debug("Resolved instance: %s\n", final->repr(final));
                    *code = 1;
                    return final;
                }
            } else if (resolved && td.t == td_instance && is_callable(td.instance->image)) {
                *code = 0;
                return resolve_via_signature(name, Generics, ((struct PredicateImage*)td.instance->image)->header, resolved, scope);
            }
        }
    }
    *code = 0;
    return NULL;
}

#define APPLY(td, Generics, nesting, scope) do {  \
        if (td.t == td_generic) { \
            int idx = count_generic_place_idx(td.generic); \
            if (Generics[idx]) {    \
                td.t = td_instance; \
                td.instance = get_nested_instance(Generics[idx], nesting, scope); \
            } \
        } else if (td.t == td_instance && is_callable(td.instance->image)) { \
            struct PredicateImage *nimg = (struct PredicateImage*)td.instance->image; \
            td.instance->image = predicate_apply_generics(nimg, Generics, scope); \
            td.instance->doc = td.instance->repr(td.instance); /* instance representation is cached in doc */ \
        } \
    } while (0)

struct PredicateImage *predicate_apply_generics(
    struct PredicateImage *pim,
    Instance **Generics,
    laure_scope_t *scope
) {
    struct PredicateImage *image = predicate_copy(pim);
    for (int i = 0; i < image->header.args->length; i++) {
        APPLY(image->header.args->data[i], Generics, image->header.nestings[i], scope);
    }
    if (image->header.resp) {
        laure_typedecl td = *image->header.resp;
        APPLY(td, Generics, image->header.response_nesting, scope);
        *image->header.resp = td;
    }
    return image;
}

// Predicate call argument procession
/* Processes expressions passed as a predicate argument 
   and passes new instances into predicate argument recorder.
1) logic variable
   * bound link if exists
   * creates new if not
   * processes anonymous instances
2) translatable expression
   * passes translatable expression into a image-specific translator
If recorder is null, then only type check should be performed 
and temp instance must be deleted in place.
*/
#define ARGPROC_RES string
#define ARGPROC_RET_FALSE (void*)1
#define ARGPROC_RES_(R, OK, ERROR, RETFALSE, err) do {string err = NULL; if (R == 0) OK else if (R == ARGPROC_RET_FALSE) {RETFALSE} else {err = (string)R; ERROR;}} while (0)
#define RECORDER_T(name) void (*name)(Instance *ins, ulong link, struct arg_rec_ctx *ctx)
#define return_str_fmt(fmt, ...) do {char sstr[128]; snprintf(sstr, 128, fmt, __VA_ARGS__); string str = strdup( sstr ); return str;} while (0)
#define isanonvar(__name) (str_eq(__name, ANONVAR_NAME))
ARGPROC_RES pred_call_procvar(
    predfinal          *pf, 
    control_ctx        *cctx, 

    laure_scope_t      *prev_scope, 
    laure_scope_t      *new_scope, 

    string              argn, 
    laure_typedecl     *hint_opt, 
    
    laure_expression_t *exp, 
    RECORDER_T         (recorder), 
    struct arg_rec_ctx *ctx, 
    bool                create_copy,

    Instance **Generics,
    uint nesting,
    struct PredicateImage *pred_img,
    bool allow_locked_mutability,
    bool *is_inst
) {
    switch (exp->t) {
        case let_name: {
            // name of var in prev_scope
            string vname = exp->s;
            if (isanonvar(vname))
                vname = laure_scope_generate_unique_name();
            
            ulong link[1];
            link[0] = 0;
            Instance *arg = laure_scope_find_by_key_l(prev_scope, vname, link, true);

            if (arg && arg->locked && ! allow_locked_mutability) {
                return_str_fmt("%s is locked", arg->name);
            } else if (! arg) {
                if (hint_opt) {
                    Instance *hint;
                    if (hint_opt->t == td_generic) {
                        Instance *T = Generics[count_generic_place_idx(hint_opt->generic)];
                        assert(T);
                        hint = get_nested_instance(T, nesting, prev_scope);
                    } else {
                        hint = hint_opt->instance;

                        if (hint && hint->image && read_head(hint->image).t == PREDICATE_FACT) {
                            struct PredicateImage *pim = (struct PredicateImage*)hint->image;
                            if (pim->header.resp && pim->header.resp->t == td_auto) {
                                // hint is uuid
                                arg = laure_create_uuid_instance(argn, pim->bound, NULL);
                                goto linking;
                            } else {
                                // create new header
                                struct PredicateImage *pim_new = predicate_apply_generics(pim, Generics, new_scope);
                                arg = instance_new(argn, NULL, pim_new);
                                arg->repr = hint->repr;
                                goto linking;
                            }
                        }
                    }
                    if (! hint) {
                        return_str_fmt("arg %s is undefined, can't resolve", vname);
                    }
                    arg = instance_new_copy(argn, hint, new_scope);
                } else {
                    return_str_fmt("specification of %s is needed", vname);
                }
                linking: {};
                ulong preset_link = 0;
                if (prev_scope->idx == 1) {
                    ulong l[1];
                    Instance *glob = laure_scope_find_by_key_l(cctx->tmp_answer_scope, vname, l, false);
                    if (! glob) {
                        Instance *nins = instance_deepcopy(vname, arg);
                        #ifdef SCOPE_LINKED
                        linked_scope_t *linked = laure_scope_insert(cctx->tmp_answer_scope, nins);
                        // laure_add_grabbed_link(cctx, linked->link);
                        *l = linked->link;
                        #else
                        laure_cell cell = laure_scope_insert(cctx->tmp_answer_scope, nins);
                        // laure_add_grabbed_link(cctx, cell.link);
                        *l = cell.link;
                        #endif
                    }
                    preset_link = l[0];
                }

                Instance *prev_ins = instance_deepcopy(vname, arg);

                if (preset_link > 0) {
                    laure_scope_insert_l(prev_scope, prev_ins, preset_link);
                    *link = preset_link;
                } else {
                    ulong lin = laure_scope_generate_link();
                    laure_scope_insert_l(prev_scope, prev_ins, lin);
                    *link = lin;
                }
                if (! new_scope) arg = prev_ins;
            } else {
                if (hint_opt && pf->t == PF_INTERIOR) {
                    // check variable corresponds the type
                    Instance *hint;
                    if (hint_opt->t == td_generic) {
                        Instance *T = Generics[count_generic_place_idx(hint_opt->generic)];
                        assert(T);
                        hint = get_nested_instance(T, nesting, prev_scope);
                    } else
                        hint = hint_opt->instance;
                    
                    if (hint && hint->image && arg->image) {
                        void *Tim_copy = image_deepcopy(hint->image);
                        if (! (read_head(Tim_copy).t == PREDICATE_FACT))
                            if (! image_equals(arg->image, Tim_copy, prev_scope))
                                return ARGPROC_RET_FALSE;
                        image_free(Tim_copy);
                    }
                }
                if (read_head(arg->image).t == LINKED) {
                    arg = linked_resolve(arg->image, prev_scope);
                }
                Instance *existing_same = new_scope ? laure_scope_find_by_link(new_scope, *link, false) : NULL;
                if (existing_same && recorder) {
                     arg = instance_new(argn, MARKER_NODELETE, existing_same->image);
                } else if (create_copy && recorder)
                    arg = instance_new_copy(argn, arg, prev_scope);
            }

            if (arg && is_inst)
                *is_inst = instantiated(arg);
            
            ulong lin;
            if (link[0])
                lin = link[0];
            else {
                lin = laure_scope_generate_link();
            }
            if (recorder)
                recorder(arg, lin, ctx);
            
            if (prev_scope->idx == 1) {
                Instance *glob = laure_scope_find_by_key(cctx->tmp_answer_scope, vname, false);
                if (! glob) {
                    Instance *nins = instance_deepcopy(vname, arg);
                    #ifdef SCOPE_LINKED
                    linked_scope_t *linked = laure_scope_insert(cctx->tmp_answer_scope, nins);
                    *l = linked->link;
                    #else
                    laure_cell cell = laure_scope_insert_l(cctx->tmp_answer_scope, nins, lin);
                    #endif
                }
            }
            break;
        }
        default: {
            if (! hint_opt)
                return_str_fmt("cannot resolve meaning of %s; add hint", exp->s);
            Instance *hint;
            if (hint_opt->t == td_generic) {
                Instance *T = Generics[count_generic_place_idx(hint_opt->generic)];
                assert(T);
                hint = get_nested_instance(T, nesting, prev_scope);;
            } else {
                hint = hint_opt->instance;
            }
            if (! hint) {
                return_str_fmt("hint is undefined for %s [%s]", exp->fullstring, EXPT_NAMES[exp->t]);
            }
            Instance *arg = instance_new_copy(argn, hint, new_scope);
            
            if (! arg->image)
                return_str_fmt("specification of %s is needed", argn);
            
            ulong predeflink = laure_scope_generate_link();

            laure_image_head head = read_head(arg->image);
            bool result = head.translator->invoke(exp, arg->image, prev_scope, predeflink);
            if (is_inst)
                *is_inst = true;

            if (! result) {
                image_free(arg->image);
                laure_free(arg);
                return ARGPROC_RET_FALSE;
            }

            if (head.t == PREDICATE_FACT) {
                laure_free(arg);
                if (recorder) {
                    Instance *uu_ins = laure_create_uuid_instance(argn, hint->name, exp->s);
                    recorder(uu_ins, laure_scope_generate_link(), ctx);
                }
                break;
            }

            if (recorder)
                recorder(arg, predeflink, ctx);
            else {
                image_free(arg->image);
                laure_free(arg);
            }
            break;
        }
    }
    return NULL;
}

void cpred_arg_recorder(Instance *ins, ulong link, struct arg_rec_ctx *ctx) {
    struct predicate_arg pa = {ctx->idx_pd, ins, true, link};
    preddata_push(ctx->pd, pa);
}

void dompred_arg_recorder(Instance *ins, ulong link, struct arg_rec_ctx *ctx) {
    laure_scope_insert_l(ctx->new_scope, ins, link);
}

void cpred_resp_recorder(Instance *ins, ulong link, struct arg_rec_ctx *ctx) {
    ctx->pd->resp = ins;
    ctx->pd->resp_link = link;
}

void pd_show(preddata *pd) {
    for (uint i = 0; i < pd->argc; i++) {
        Instance *ins = pd->argv[i].arg;
        printf("|%d| %s (%lu)\n", i, ins->repr(ins), pd->argv[i].link_id);
    }
    if (pd->resp) {
        Instance *ins = pd->resp;
        printf("resp %s (%lu)\n", ins->repr(ins), pd->resp_link);
    }
}

bool check_namespace(laure_scope_t *scope, Instance *T, laure_expression_t *ns) {
    if (ns->t == let_name) {
        Instance *var = laure_scope_find_var(scope, ns, true);
        if (! var) return false;
        if (read_head(var->image).t != read_head(T->image).t)
            return false;
        void *cp_img = image_deepcopy(var->image);
        void *cpT_img = image_deepcopy(T->image);
        bool result = image_equals(cp_img, cpT_img, scope);
        image_free(cp_img);
        image_free(cpT_img);
        return result;
    } else if (ns->t == let_singlq)
        // abstract generic always true
        return true;
    else if (ns->t == let_atom_sign) {
        return read_head(T->image).t == ATOM;
    }
    assert(false);
}

int generic_process(
    Instance *Generics[GENERIC_PLACES], 
    laure_typedecl td,
    laure_expression_t *e,
    laure_scope_t *scope,
    size_t ax,
    struct PredicateHeaderImage header,
    string predicate_name
) {
    if (td.t != td_generic) return 0;
    int place = count_generic_place_idx(td.generic);
    if (! Generics[place]) {
        Instance *T = NULL;
        if (e->link && e->link->ba->set) {
            // find generic by name
            string n = NULL;
            uint nesting = 0;
            if (e->link->ba->body_len == 1 && e->link->ba->set->expression->t == let_name) {
                // all generic should be this type
                n = e->link->ba->set->expression->s;
                nesting = e->link->ba->set->expression->flag;
                T = laure_scope_find_by_key(scope->glob, n, true);
                if (T) {
                    T = get_nested_instance(T, nesting, scope->glob);
                    T = instance_shallow_copy(T);
                }
                else
                    return 2;
            } else {
                laure_expression_set *set = e->link->ba->set;
                while (set) {
                    if (set->expression->t == let_name) {
                        T = laure_scope_find_by_key(scope->glob, set->expression->s, true);
                        if (T) {
                            T = get_nested_instance(T, nesting, scope->glob);
                            T = instance_shallow_copy(T);
                        }
                        else
                            return 2;
                        bool found = false;
                        for (int i = 0; i < header.args->length; i++) {
                            if (header.args->data->t == td_generic) {
                                if (! Generics[count_generic_place_idx(header.args->data->generic)]) {
                                    Generics[count_generic_place_idx(header.args->data->generic)] = T;
                                    found = true;
                                    break;
                                }
                            }
                        }
                        if (! found && header.resp && header.resp->t == td_generic) {
                            Generics[count_generic_place_idx(header.resp->generic)] = T;
                            found = true;
                        }
                    } else if (set->expression->t == let_assert) {
                        char *tn = set->expression->ba->set->expression->s,
                             *cn = set->expression->ba->set->next->expression->s;
                        nesting = set->expression->ba->set->next->expression->flag;
                        int name = (int)tn[0];
                        Instance *instance = laure_scope_find_by_key(scope, cn, true);
                        if (! instance)
                            return 2;
                        T = get_nested_instance(instance, nesting, scope->glob);
                        T = instance_shallow_copy(T);
                        Generics[count_generic_place_idx(name)] = T;
                    } else {
                        return 10;
                    }
                    set = set->next;
                }
                if (Generics[place])
                    return 0;
                else
                    T = NULL;
            }
        }
        if (! T) {
            int code[1];
            T = resolve_generic_T(td.generic, Generics, header, e->ba->set, scope, code);
            
            if (! T && ! *code) {
                string dname = laure_alloc(strlen(predicate_name) + 3);
                strcpy(dname, "T:");
                strcat(dname, predicate_name);
                string Tdefault = get_dflag(dname);
                laure_free(dname);
                if (Tdefault) {
                    T = laure_scope_find_by_key(scope, Tdefault, true);
                    T = instance_shallow_copy(T);
                    if (! T)
                        return 1;
                } else
                    return 3;
            } else if (! T) {
                return 4;
            }
        }
        Generics[place] = T;
    }
    return 0;
}

void show_generics(Instance **Generics) {
    for (int i = 0; i < GENERIC_PLACES; i++) {
        if (Generics[i]) {
            printf("%c: %s\n", GENERIC_FIRST + i, Generics[i]->repr(Generics[i]));
        }
    }
}

/* =-------=
Predicate/constraint call.
=-------= */
#define pred_call_cleanup do {laure_scope_free(init_scope); laure_scope_free(prev);} while (0)
qresp laure_eval_pred_call(_laure_eval_sub_args) {
    assert(e->t == let_pred_call);
    UNPACK_CCTX(cctx);
    /* MEMGUARD_CLEANUP_CONTEXT(cleanup); */
    
    // Check recursion depth to prevent stack overflow
    if (cctx->recursion_depth >= cctx->max_recursion_depth) {
        RESPOND_ERROR(runtime_err, e, "Maximum recursion depth (%u) exceeded. Possible infinite recursion detected.", cctx->max_recursion_depth);
    }
    
    // Increment recursion depth
    cctx->recursion_depth++;
    
    qresp result; // To hold return value for cleanup
    string predicate_name = e->s;
    if (str_eq(e->s, RECURSIVE_AUTONAME)) {
        if (! scope->owner) 
            RESPOND_ERROR_DECR_DEPTH(cctx, syntaxic_err, e, "autoname can only be used inside other predicates%s", "");
        predicate_name = scope->owner;
    }
    Instance *predicate_ins = laure_scope_find_by_key(scope, predicate_name, true);
    if (! predicate_ins)
        RESPOND_ERROR_DECR_DEPTH(cctx, undefined_err, e, "predicate %s", predicate_name);
    enum ImageT call_t = read_head(predicate_ins->image).t;
    if (! (call_t == PREDICATE_FACT || call_t == CONSTRAINT_FACT))
        RESPOND_ERROR_DECR_DEPTH(cctx, instance_err, e, "%s is neither predicate nor constraint", predicate_name);
    bool is_constraint = (call_t == CONSTRAINT_FACT);
    bool tfc = is_constraint && !qctx->constraint_mode;

    struct PredicateImage *pred_img = (struct PredicateImage*) predicate_ins->image;
    if (pred_img->is_primitive)
        RESPOND_ERROR(too_broad_err, e, "primitive (%s) call is prohibited before instantiation; unify", predicate_name);
    if (is_constraint)
        qctx->constraint_mode = true;
    if (pred_img->variations->finals == NULL) {
        // first call constructs finals
        void **pfs = laure_alloc(sizeof(void*) * pred_img->variations->len);
        for (uint i = 0; i < pred_img->variations->len; i++) {
            struct PredicateImageVariation pv = pred_img->variations->set[i];
            predfinal *pf = get_pred_final(pred_img, pv);
            pfs[i] = pf;
        }
        pred_img->variations->finals = (predfinal**)pfs;
    }

    bool need_more = false;
    bool found = false;
    uint backtrace_cursor = LAURE_BACKTRACE ? LAURE_BACKTRACE->cursor : 0;
    ulong cut_store = cctx->cut;  // Store initial cut state

    laure_scope_t *init_scope = laure_scope_create_copy(cctx, scope);
    for (uint variation_idx = 0; variation_idx < pred_img->variations->len; variation_idx++) {
        predfinal *pf = pred_img->variations->finals[variation_idx];
        Instance *uuid_instance = NULL;
        laure_scope_t *prev = laure_scope_create_copy(cctx, init_scope);

        if (pf->t == PF_INTERIOR && pred_img->header.resp && pred_img->header.resp->t == td_auto) {
            if (e->ba->has_resp) {
                // variation is known by uuid
                laure_expression_t *resp_expression = (
                    laure_expression_set_get_by_idx(
                        e->ba->set,
                        e->ba->body_len
                    )
                );
                resp_expression->s = rough_strip_string(
                    resp_expression->s
                );
                uuid_t uu;
                if (resp_expression->t == let_name) {
                    Instance *uuid_ins = laure_scope_find_var(prev, resp_expression, true);
                    if (uuid_ins) {
                        enum ImageT uuid_imt = read_head(uuid_ins->image).t;
                        if (uuid_imt == PREDICATE_FACT) {
                            // bound but not set
                            struct PredicateImage *pim = (struct PredicateImage*)uuid_ins->image;
                            if (! str_eq(pim->bound, predicate_name)) return RESPOND_FALSE;
                            force_predicate_to_uuid(pim);
                            goto with_instance;
                        } else {
                            if (uuid_imt != UUID)
                                RESPOND_ERROR(instance_err, resp_expression, "%s is not instance of UUID / Predicate (but of %s)", resp_expression->s, IMG_NAMES[read_head(uuid_ins->image).t]);
                        
                            laure_uuid_image *uu_image = (laure_uuid_image*)uuid_ins->image;
                            if (! str_eq(uu_image->bound, predicate_name)) return RESPOND_FALSE;
                            if (uu_image->unset) {
                                uuid_copy(uu_image->uuid, pf->uu);
                                uuid_copy(uu, pf->uu);
                                uu_image->unset = false;
                            } else {
                                uuid_copy(uu, uu_image->uuid);
                            }
                        }
                    } else {
                        with_instance:
                        uuid_ins = instance_new(resp_expression->s, NULL, laure_create_uuid(predicate_name, pf->uu));
                        uuid_ins->repr = uuid_repr;
                        uuid_copy(uu, pf->uu);
                    }
                    uuid_instance = uuid_ins;
                    
                } else if (resp_expression->t == let_data) {
                    if (uuid_parse(resp_expression->s, uu) != 0)
                        RESPOND_ERROR(syntaxic_err, resp_expression, "cannot parse UUID");
                } else
                    RESPOND_ERROR(
                        syntaxic_err, 
                        resp_expression, 
                        "cannot use %s as ID", 
                        EXPT_NAMES[resp_expression->t]
                    );
                
                int cmp = uuid_compare(uu, pf->uu);
                if (cmp != 0) continue;
            }
        }

        qresp resp;
        bool do_continue = true;
        bool cut_case = false;
        
        debug("predicate call %s: entering variation %u with cut_store=%lu, current_cut=%lu, scope_idx=%lu\n", 
              predicate_name, variation_idx, cut_store, cctx->cut, scope->idx);

        if (pf->t == PF_C) {
            // C source predicate
            if (e->ba->body_len != pf->c.argc && pf->c.argc != -1) {
                laure_scope_free(init_scope);
                RESPOND_ERROR(signature_err, e, "predicate %s got %d args, expected %d", predicate_name, e->ba->body_len, pf->c.argc);
            }
            preddata *pd = preddata_new(prev);
            struct arg_rec_ctx actx[1];
            actx->pd = pd;
            actx->prev_scope = prev;

            laure_expression_set *arg_l = e->ba->set;
            for (uint idx = 0; idx < e->ba->body_len; idx++) {
                actx->idx_pd = idx;
                laure_expression_t *argexp = arg_l->expression;

                laure_typedecl decl[1];
                if (pf->c.hints[idx]) {
                    decl->t = td_instance;
                    decl->instance = pf->c.hints[idx]->hint;
                }

                ARGPROC_RES res = pred_call_procvar(
                    pf, cctx,
                    prev, NULL,
                    argexp->s, 
                    pf->c.hints[idx] ? decl : NULL, 
                    argexp, cpred_arg_recorder, 
                    actx, false, NULL, 0, pred_img, false, NULL);

                ARGPROC_RES_(
                    res, 
                    {}, 
                    {pred_call_cleanup; return respond(q_error, err);}, 
                    {pred_call_cleanup; return RESPOND_FALSE;},
                    err);
                
                arg_l = arg_l->next;
            }

            if (arg_l) {
                laure_expression_t *exp = arg_l->expression;
                if (! exp) {
                    Instance *hint = pf->c.resp_hint;
                    if (! hint)
                        RESPOND_ERROR(signature_err, e, "specification of %s's response is needed", predicate_name);
                    pd->resp = instance_deepcopy(MOCK_NAME, hint);
                    pd->resp_link = 0;
                } else {
                    actx->idx_pd = 0;

                    laure_typedecl decl[1];
                    if (pf->c.resp_hint) {
                        decl->t = td_instance;
                        decl->instance = pf->c.resp_hint;
                    }

                    ARGPROC_RES res = pred_call_procvar(
                        pf, cctx, 
                        prev, NULL,
                        exp->s,
                        pf->c.resp_hint ? decl : NULL,
                        exp, cpred_resp_recorder,
                        actx, false, NULL, 0, pred_img, true, NULL);
                    
                    ARGPROC_RES_(
                        res, 
                        {}, 
                        {pred_call_cleanup; return respond(q_error, err);},
                        {pred_call_cleanup; return RESPOND_FALSE;},
                        err);
                }
            } else {
                pd->resp = NULL;
                pd->resp_link = 0;
            }

            resp = pf->c.pred(pd, cctx);

            if (resp.state == q_bag_full) {
                // from __bag_sz predicate
                assert(qctx->next->bag);
                resp.state = (resp.payload) ? q_true : q_false;
                cctx->this_break = true;
            } else if (resp.state == q_instantiate_first) {
                laure_expression_set nset[1];
                nset->expression = e;
                nset->next = expset;
                qcontext nqctx[1] = {qcontext_temp(qctx->next, nset, qctx->bag)};

                uintptr_t argument_idx = (uintptr_t)resp.payload;
                ulong link = pd_get_arg_link(pd, (int)argument_idx);

                Instance *arg = laure_scope_find_by_link(prev, link, true);
                assert(arg);

                if (arg->locked)
                    RESPOND_ERROR(instance_err, e, "instantiation requested for %lu argument but it is locked", argument_idx);
                
                cctx->qctx = nqctx;
                cctx->scope = prev;
                struct img_rec_ctx *ctx = img_rec_ctx_create(arg, cctx, nset, qr_process_default, link);
                gen_resp gr = instance_generate(prev, arg, image_rec_default, ctx);
                cctx->qctx = qctx;
                cctx->scope = scope;
                laure_free(ctx);

                resp = gen_resp_process(gr);
            }

            preddata_free(pd);

            if (resp.state == q_false) continue;

            if (tfc) {
                qctx->constraint_mode = false;
                if (qctx->next)
                    qctx->next->constraint_mode = false;
            }

            if (resp.state == q_stop || resp.state == q_error) {
                pred_call_cleanup;
                return resp;
            } else if (resp.state == q_yield) {
                if (resp.payload == YIELD_OK) {
                    found = true;
                }
                continue;
            }

            laure_expression_set *full = qctx->expset;
            qctx->expset = expset;
            cctx->scope = prev;
            resp = laure_start(cctx, expset);
            cctx->scope = scope;
            qctx->expset = full;
        } else {

            if (e->ba->body_len < pf->interior.argc) {
                laure_scope_free(init_scope);
                RESPOND_ERROR(signature_err, e, "predicate %s got %d args, expected %d", predicate_name, e->ba->body_len, pf->interior.argc);
            }

            laure_expression_t *exp = pred_img->variations->set[variation_idx].exp;
            cut_case = PREDFLAG_IS_CUT(exp->flag);
            if (cut_case)
                debug("predicate variation will result in global cut if succeed");

            // resolve generic type
            Instance *Generics[GENERIC_PLACES];
            memset(Generics, 0, sizeof(Instance*) * GENERIC_PLACES);

            // resolve namespace
            if (exp->link) {
                Instance *T = Generics[count_generic_place_idx('T')];
                if (! T)
                    RESPOND_ERROR(internal_err, e, "namespace declaration can be used only within generic type");
                
                bool is_valid = false;
                if (exp->link->t == let_array) {
                    // union
                    laure_expression_t *ptr;
                    EXPSET_ITER(exp->link->ba->set, ptr, {
                        if (check_namespace(scope, T, ptr)) {
                            is_valid = true;
                            break;
                        }
                    });
                } else
                    is_valid = check_namespace(scope, T, exp->link);
                
                if (! is_valid) continue;
            }

            // isolated {
            if (! laure_typeset_all_instances(pred_img->header.args)) {
                int code;
                for (int ax = 0; ax < pred_img->header.args->length; ax++) {
                    code = generic_process(Generics, pred_img->header.args->data[ax], e, scope, ax, pred_img->header, predicate_name);
                    if (code != 0)
                        break;
                }
                if (pred_img->header.resp && code == 0) {
                    code = generic_process(Generics, *pred_img->header.resp, e, scope, pred_img->header.args->length, pred_img->header, predicate_name);
                }
                if (code != 0) {
                    if (code == 1) {
                        RESPOND_ERROR(undefined_err, e, "default T which was set for predicate %s is undefined", predicate_name);
                    } else if (code == 2) {
                        RESPOND_ERROR(undefined_err, e, "instance is undefined, can't resolve the generic type");
                    } else if (code == 3) {
                        RESPOND_ERROR(signature_err, e, "unable to resolve generic datatype; add explicit cast or add hint to resolve");
                    } else if (code == 10) {
                        RESPOND_ERROR(internal_err, e, "invalid expression used as type clarification");
                    } else if (code == 4) {
                        return RESPOND_FALSE;
                    }
                }
            }

            laure_scope_t *nscope = laure_scope_new(scope->glob, prev);
            nscope->owner = predicate_ins->name;

            // Generic type instances are copied to local scope (locked)
            for (int place = 0; place < GENERIC_PLACES; place++) {
                if (Generics[place]) {
                    char name = GENERIC_FIRST + place;
                    string name_str = laure_alloc(2);
                    *name_str = name;
                    name_str[1] = 0;
                    Instance *GI = instance_deepcopy(name_str, Generics[place]);
                    GI->locked = true;
                    laure_scope_insert(nscope, GI);
                }
            }

            struct arg_rec_ctx actx[1];
            actx->new_scope = nscope;

            bool argi[32], respi;

            laure_expression_set *arg_l = e->ba->set;
            for (uint idx = 0; idx < e->ba->body_len; idx++) {
                actx->idx_pd = idx;
                laure_expression_t *argexp = arg_l->expression;

                if (idx == pf->interior.argc && pf->interior.respn) {
                    goto joint_response;
                } else if (idx > pf->interior.argc) {
                    RESPOND_ERROR(signature_err, e, "too much arguments (expected %d)", pf->interior.argc);
                }

                RECORDER_T(rec) = NULL;
                if (! str_eq(pf->interior.argn[idx], "_")) {
                    rec = dompred_arg_recorder;
                }

                ARGPROC_RES res = pred_call_procvar(
                    pf, cctx,
                    prev, nscope, 
                    laure_get_argn(idx), 
                    &pred_img->header.args->data[idx], 
                    argexp, rec, 
                    actx, true, Generics, pred_img->header.nestings[idx], pred_img, false, argi + idx);

                ARGPROC_RES_(
                    res, 
                    {}, 
                    {pred_call_cleanup; return respond(q_error, err);}, 
                    {pred_call_cleanup; return RESPOND_FALSE;},
                    err);

                arg_l = arg_l->next;
            }

            if (pf->interior.respn && pred_img->header.resp->t != td_auto) {
                joint_response: {};
                if (! arg_l) {
                    Instance *hint = NULL;
                    if (pred_img->header.resp) {
                        if (pred_img->header.resp->t == td_generic) {
                            hint = get_nested_instance(Generics[count_generic_place_idx(pred_img->header.resp->generic)], pred_img->header.response_nesting, scope);
                        } else {
                            hint = pred_img->header.resp->instance;
                        }
                    }
                    if (! hint)
                        RESPOND_ERROR(signature_err, e, "specification of %s's response is needed", predicate_name);
                    if (! str_eq(pf->interior.respn, "_")) {
                        Instance *rins = instance_deepcopy(laure_get_respn(), hint);
                        laure_scope_insert(nscope, rins);
                    }
                    
                } else {
                    actx->idx_pd = 0;
                    laure_expression_t *exp = arg_l->expression;

                    RECORDER_T(rec) = NULL;
                    if (! str_eq(pf->interior.respn, "_"))
                        rec = dompred_arg_recorder;

                    ARGPROC_RES res = pred_call_procvar(
                        pf, cctx,
                        prev, nscope,
                        laure_get_respn(),
                        pred_img->header.resp,
                        exp, rec,
                        actx, true, Generics, pred_img->header.response_nesting, pred_img, false, &respi);
                    
                    ARGPROC_RES_(
                        res,
                        {},
                        {pred_call_cleanup; return respond(q_error, err);},
                        {pred_call_cleanup; return RESPOND_FALSE;},
                        err);
                }
            } else
                respi = false;
            
            for (uint i = 0; i < GENERIC_PLACES; i++) laure_free(Generics[i]);

            if (uuid_instance) {
                ulong l[1];
                *l = 0;
                if (prev->idx == 1) {
                    Instance *glob = laure_scope_find_by_key_l(cctx->tmp_answer_scope, uuid_instance->name, l, false);
                    if (! glob) {
                        Instance *nins = instance_shallow_copy(uuid_instance);
                        nins->image = NULL;
                        ulong lin = laure_scope_generate_link();
                        laure_scope_insert_l(cctx->tmp_answer_scope, nins, lin);
                        *l = lin;
                    }
                }
                if (! *l) *l = laure_scope_generate_link();
                laure_scope_insert_l(prev, uuid_instance, *l);
            }
            /*
            laure_expression_set *body = pf->interior.body;
            body = laure_expression_compose(body);
            */
            laure_expression_set *body =
            #ifndef DISABLE_ORDERING
            laure_get_ordered_predicate_body(pf->interior.plp, e->ba->body_len, argi, respi);
            #ifdef DEBUG
            debug("ordered body\n");
            expression_set_show(body);
            #endif
            #else
            pf->interior.body;
            #endif
            
            laure_expression_t *ptr;

            qcontext *nqctx = qcontext_new(body);
            laure_expression_set *full_expset = qctx->expset;
            
            #ifndef DISABLE_WS
            laure_ws *old_ws = cctx->ws;
            #endif

            qctx->expset = expset;
            nqctx->next = qctx;
            nqctx->constraint_mode = is_constraint;

            cctx->scope = nscope;
            cctx->qctx = nqctx;
            #ifndef DISABLE_WS
            if (LAURE_WS)
                cctx->ws = laure_ws_create(old_ws);
            #endif

            resp = laure_start(cctx, body);

            #ifndef DISABLE_WS
            if (LAURE_WS) {
                laure_ws_free(cctx->ws);
                cctx->ws = old_ws;
            }
            #endif

            qctx->expset = full_expset;
            cctx->qctx = qctx;
            cctx->scope = scope;
        }
        if (resp.state == q_error || resp.state == q_stop) {
            return resp;
        } else if (cctx->cut - 1 <= scope->idx) {
            found = true;
            debug("variation of %s skipped (due to cut: up to scope idx=%ld)\n", predicate_name, cctx->cut);
            if (LAURE_BACKTRACE) LAURE_BACKTRACE->cursor = backtrace_cursor;
            break;
        } else if (cctx->cut != 0 && cctx->cut - 1 > scope->idx) {
            debug("stop cutting on predicate %s\n", predicate_name);
            cctx->cut = 0;
            if (LAURE_BACKTRACE) LAURE_BACKTRACE->cursor = backtrace_cursor;
        } else if (resp.state == q_true || resp.state == q_continue) {
            found = true;
            if (LAURE_BACKTRACE) LAURE_BACKTRACE->cursor = backtrace_cursor;
            if (cut_case) break;
        } else if (resp.state == q_yield) {
            if (resp.payload == YIELD_OK) {
                found = true;
                if (LAURE_BACKTRACE) LAURE_BACKTRACE->cursor = backtrace_cursor;
                if (cut_case) break;
            }
        }
        // laure_scope_free(prev);
    }
    // laure_scope_free(init_scope);
    
    // Restore cut state if no cut was triggered in this predicate
    if (cctx->cut == 0) {
        cctx->cut = cut_store;
        debug("predicate call %s: restoring cut state to %lu\n", predicate_name, cut_store);
    } else {
        debug("predicate call %s: leaving cut active at %lu\n", predicate_name, cctx->cut);
    }
    
    cctx->recursion_depth--; // Decrement recursion depth before return
    return RESPOND_YIELD(found ? YIELD_OK : YIELD_FAIL);
}

/* =-------=
Declaration of callable instance
=-------= */
qresp laure_eval_callable_decl(_laure_eval_sub_args) {
    assert(e->t == let_pred || e->t == let_constraint);
    UNPACK_CCTX(cctx);
    laure_session_t session[1];
    session->scope = scope;
    memset(session->_included_filepaths, 0, sizeof(void*) * included_fp_max);
    apply_result_t result = laure_consult_predicate(session, scope, e, LAURE_CURRENT_ADDRESS);
    if (result.status)
        return RESPOND_TRUE;
    else
        return respond(q_error, result.error);
}

gen_resp qr_process_quant_all(qresp qr, struct img_rec_ctx *ctx) {
    if (qr.state == q_error) {
        gen_resp gr;
        gr.r = 0;
        gr.qr = qr;
        return gr;
    } else {
        if (qr.state == q_yield) {
            gen_resp gr;
            gr.r = qr.payload != YIELD_FAIL;
            if (gr.r)
                gr.qr = respond(q_true, 0);
            else
                gr.qr = respond(q_false, 0);
            return gr;
        } else {
            gen_resp gr;
            gr.r = qr.state == q_true;
            gr.qr = qr;
            return gr;
        }
    }
}

gen_resp qr_process_quant_exists(qresp qr, struct img_rec_ctx *ctx) {
    if (qr.state == q_error) {
        gen_resp gr;
        gr.r = 0;
        gr.qr = qr;
        return gr;
    } else {
        if (qr.state == q_yield) {
            if (qr.payload == YIELD_OK) {
                gen_resp gr;
                gr.r = 0;
                gr.qr = RESPOND_TRUE;
                return gr;
            }
            gen_resp gr;
            gr.r = 1;
            gr.qr = RESPOND_FALSE;
            return gr;
        } else {
            gen_resp gr;
            gr.r = qr.state != q_true;
            gr.qr = qr;
            return gr;
        }
    }
}

/* =-------=
Quantified expression
=-------= */
qresp laure_eval_quant(_laure_eval_sub_args) {
    assert(e->t == let_quant);
    UNPACK_CCTX(cctx);
    string vname = e->s;
    laure_expression_set *set = e->ba->set;

    ulong link[1];
    Instance *ins = laure_scope_find_by_key_l(scope, vname, link, false);
    if (! ins)
        RESPOND_ERROR(undefined_err, e, "name %s", vname);
    
    bool dp = cctx->vpk->do_process;
    
    qcontext nqctx[1];
    *nqctx = qcontext_temp(NULL, set, NULL);

    cctx->qctx = nqctx;

    vpk->do_process = false;

    qresp qr = respond(q_true, 0);
    switch (e->flag) {
        case 1: {
            // quantifier <All>
            struct img_rec_ctx ctx[1];
            ctx->cctx = cctx;
            ctx->expset = set;
            ctx->var = ins;
            ctx->qr_process = qr_process_quant_all;
            ctx->link = *link;
            gen_resp gr = instance_generate(scope, ins, image_rec_default, ctx);
            if (! gr.r) qr = respond(q_false, 0);
            break;
        }
        case 2: {
            // quantifier <Exists>
            struct img_rec_ctx ctx[1];
            ctx->cctx = cctx;
            ctx->expset = set;
            ctx->var = ins;
            ctx->qr_process = qr_process_quant_exists;
            gen_resp gr = instance_generate(scope, ins, image_rec_default, ctx);
            if (gr.qr.state == q_error) qr = gr.qr;
            else if (gr.r) qr = respond(q_false, 0);
            else qr = respond(q_true, 0);
            break;
        }
        default:
            RESPOND_ERROR(undefined_err, e, "quantifier %s (id=%d)", e->s, e->flag);
    }
    cctx->scope = scope;
    cctx->qctx = qctx;
    vpk->do_process = dp;
    return qr;
}

bool in_next(qcontext *search, qcontext *from) {
    qcontext *curr = search;
    while (curr) {
        if (curr == from) return true;
        curr = curr->next;
    }
    return false;
}

/* =-------=
Implication
=-------= */
qresp laure_eval_imply(_laure_eval_sub_args) {
    assert(e->t == let_imply);
    UNPACK_CCTX(cctx);
    laure_expression_t *fact = e->ba->set->expression;
    laure_expression_set *implies_for = e->ba->set->next;
    laure_expression_set *fact_set = fact->ba->set;

    laure_expression_set *old_qctx_set = qctx->expset;
    qctx->expset = expset;

    // fact_set -> implies_for

    qcontext current[1];
    *current = qcontext_temp(qctx->next, expset, qctx->bag);
    current->constraint_mode = qctx->constraint_mode;

    qcontext if_qctx[1];
    *if_qctx = qcontext_temp(current, implies_for, NULL);
    if_qctx->constraint_mode = true;

    qcontext fact_qctx[1];
    *fact_qctx = qcontext_temp(if_qctx, fact_set, NULL);
    fact_qctx->constraint_mode = true;

    laure_scope_t *nscope = laure_scope_create_copy(cctx, scope);
    nscope->repeat += 2;

    cctx->qctx = fact_qctx;
    cctx->scope = nscope;
    
    qresp qr = laure_start(cctx, fact_set);
    bool yielded = false;
    if (qr.state == q_error || qr.state == q_stop) return qr;
    else if (qr.payload == YIELD_FAIL) {
        if (fact_qctx->flagme) {
            yielded = true;
        }
    } else {
        yielded = true;
    }

    qctx->expset = old_qctx_set;
    cctx->qctx = qctx;
    cctx->scope = scope;

    if (! yielded) {
        return respond(q_true, 0);
    } else {
        return respond(q_yield, qr.payload);
    }
}

/* =-------=
Force choice
=-------= */
qresp laure_eval_choice(_laure_eval_sub_args) {
    assert(e->t == let_choice_2);
    UNPACK_CCTX(cctx);
    debug("laure_eval_choice: starting choice evaluation\n");
    laure_expression_set *set = e->ba->set;
    bool found = false;
    ulong choice_cut_store = cctx->cut;
    
    while (set) {
        laure_expression_set *choice = set->expression->ba->set;
        laure_scope_t *nscope = laure_scope_create_copy(cctx, scope);
        nscope->repeat++;

        laure_expression_set *old = qctx->expset;

        qcontext nqctx[1];
        *nqctx = qcontext_temp(qctx, choice, NULL);
        nqctx->constraint_mode = qctx->constraint_mode;

        qctx->expset = expset;

        cctx->qctx = nqctx;
        cctx->scope = nscope;
        qresp response = laure_start(cctx, choice);
        laure_scope_free(nscope);

        if (response.state == q_error) {
            debug("laure_eval_choice: choice failed with error\n");
            return response;
        }
        
        else if (response.state == q_true || (response.state == q_yield && response.payload == YIELD_OK)) {
            debug("laure_eval_choice: choice succeeded\n");
            found = true;
            // If a cut was triggered within the choice, break early
            if (cctx->cut != choice_cut_store) {
                debug("laure_eval_choice: cut detected, stopping choice evaluation\n");
                cctx->scope = scope;
                cctx->qctx = qctx;
                qctx->expset = old;
                return RESPOND_YIELD(YIELD_OK);
            }
        } else {
            debug("laure_eval_choice: choice failed\n");
        }
        
        cctx->scope = scope;
        cctx->qctx = qctx;
        qctx->expset = old;
        set = set->next;
    }
    debug("laure_eval_choice: completed, found=%s\n", found ? "true" : "false");
    return RESPOND_YIELD(YIELD_OK);
}

/* =-------=
Cutting tree
=-------= */
qresp laure_eval_cut(_laure_eval_sub_args) {
    assert(e->t == let_cut);
    UNPACK_CCTX(cctx);
    debug("laure_eval_cut: setting cut at scope index %lu (was %lu)\n", scope->idx, cctx->cut);
    cctx->cut = scope->idx;
    return respond(q_true, 0);
}

string get_work_dir_path(string f_addr) {
    if (! f_addr) return NULL;
    string naddr = strdup(f_addr);
    string ptr = naddr;
    strrev_via_swap(naddr);
    while (naddr[0] != '/') naddr++;
    strrev_via_swap(naddr);
    string n = strdup(naddr);
    free(ptr);
    return n;
}

/* =---------=
Expression set
* creates a private scope for evaluation
=---------= */
qresp laure_eval_set(_laure_eval_sub_args) {
    assert(e->t == let_set);
    UNPACK_CCTX(cctx);
    if (! e->ba->set) return respond(q_true, NULL);

    laure_expression_set *old = qctx->expset;
    bool back_do_process = cctx->vpk ? cctx->vpk->do_process : true;

    if (e->flag) {
        // Isolated set (feature #10)
        laure_scope_t *priv_scope = laure_scope_create_copy(cctx, scope);
        priv_scope->next = NULL;
        priv_scope->repeat = 0;
        qctx->expset = qctx->expset->next;

        qcontext nqctx[1];
        *nqctx = qcontext_temp(NULL, e->ba->set, NULL);
        nqctx->constraint_mode = qctx->constraint_mode;

        vpk->do_process = false;
        cctx->qctx = nqctx;
        cctx->scope = priv_scope;

        qresp response = laure_start(cctx, nqctx->expset);

        laure_scope_free(priv_scope);

        if (vpk)
            vpk->do_process = back_do_process;
        cctx->qctx = qctx;
        cctx->scope = scope;

        if (response.state == q_yield) {
            return respond((response.payload == YIELD_OK) ? q_true : q_false, NULL);
        }

        return response;
    } else {
        // Integrated set
        qctx->expset = qctx->expset->next;

        qcontext nqctx[1];
        *nqctx = qcontext_temp(qctx, e->ba->set, NULL);
        nqctx->constraint_mode = qctx->constraint_mode;

        cctx->qctx = nqctx;

        qresp response = laure_start(cctx, cctx->qctx->expset);

        qctx->expset = old;
        cctx->qctx = qctx;
        return response;
    }
}

/* =----------=
Instantiate all
=----------= */
qresp laure_eval_atom_sign(_laure_eval_sub_args) {
    assert(e->t == let_atom_sign);
    UNPACK_CCTX(cctx);

    if (cctx->scope->idx == 1) {
        RESPOND_ERROR(too_broad_err, e, "calling forced instantiation on whole scope is forbidden");
    }

    laure_scope_iter(scope, cell, {
        Instance *instance = cell->ptr;
        ulong l = cell->link;
        if (! instantiated(instance)) {
            laure_expression_set set[1];
            set->next = expset;
            set->expression = e;
            void *img = instance->image;
            struct img_rec_ctx *ctx = img_rec_ctx_create(instance, cctx, set, qr_process_default, l);
            gen_resp gr = instance_generate(scope, instance, image_rec_default, ctx);
            laure_free(ctx);
            instance->image = img;

            if (gr.r == 0) {
                return gr.qr;
            }

            return gen_resp_process(gr);
        }
    });
    return RESPOND_TRUE;
}

string string_clean(string s);
string search_path(string original_path);

/* =---------=
Writes dynamic flags
for advanced tree search
=---------= */
qresp laure_eval_command(_laure_eval_sub_args) {
    assert(e->t == let_command);
    UNPACK_CCTX(cctx);

    switch (e->flag) {
        case command_setflag: {
            string v = e->docstring;
            string n = e->s;

            #ifndef DISABLE_WS
            // check if n is one of builtin names
            if (str_eq(n, WFUNCTION_NAME)) {
                bool is_found = laure_ws_set_function_by_name(cctx->ws, v);
                if (! is_found)
                    RESPOND_ERROR(undefined_err, e, "WS function with name %s is undefined", v);
                return RESPOND_TRUE;
            }
            #endif
            else if (str_eq(n, "ordering")) {
                LAURE_FLAG_NEXT_ORDERING = true;
                return RESPOND_TRUE;
            }

            if (! v) {
                v = DUMMY_FLAGV;
            }
            add_dflag(n, v);
            return RESPOND_TRUE;
        }
        case command_error: {
            switch (e->link->t) {
                case let_name: {
                    Instance *runtime_instance = laure_scope_find_by_key(scope, e->link->s, true);
                    if (runtime_instance) {
                        RESPOND_ERROR(runtime_err, e, "error from name %s: %s", runtime_instance->name, runtime_instance->repr(runtime_instance));
                    }
                }
                default: break;
            }
            RESPOND_ERROR(runtime_err, e, "%s", e->link->s);
        }
        case command_lock_unlock: {
            string name = e->s;
            Instance *ins = laure_scope_find_by_key(scope, name, true);
            if (! ins) 
                RESPOND_ERROR(undefined_err, e, "name %s is undefined", name);
            ins->locked = e->is_header;
            return RESPOND_TRUE;
        }
        case command_use:
        case command_useso: {

            if (e->flag == command_useso) {
                string wdir = get_work_dir_path(LAURE_CURRENT_ADDRESS ? LAURE_CURRENT_ADDRESS : "./");
                char buff[1024];
                snprintf(buff, 1024, "%s%s", wdir, e->s);
                free(wdir);
                string final_path = search_path(buff);
                if (! final_path) {
                    RESPOND_ERROR(undefined_err, e, "SO %s is undefined (%s)", e->s, buff);
                }
                bool result = laure_load_shared(cctx->session, final_path);
                free(final_path);
                return result ? RESPOND_TRUE : RESPOND_FALSE;
            } else {
                assert(e->ptr);
                laure_import_mod_ll *mod_root = (laure_import_mod_ll*)e->ptr;
                for (int i = 0; i < mod_root->cnext; i++) {
                    laure_import_mod_ll *mod = mod_root->nexts[i];
                    if (! mod)
                        continue;
                    int r = laure_import_use_mod(cctx->session, mod, NULL);
                    if (r != 0)
                        RESPOND_ERROR(undefined_err, e, "failed to consult mod of %s", mod->mod);
                }
            }
            return RESPOND_TRUE;
        }
    }
    return RESPOND_TRUE;
}

/* Structure
   creates uninitted structure in scope
*/
qresp laure_eval_structure(_laure_eval_sub_args) {
    assert(e->t == let_struct);
    UNPACK_CCTX(cctx);

    if (laure_scope_find_by_key(scope, e->s, true))
        RESPOND_ERROR(runtime_err, e, "structure %s is already defined", e->s);

    laure_structure *img = laure_structure_create_header(e);
    Instance *ins = instance_new(e->s, NULL, img);
    ins->repr = structure_repr;
    laure_scope_insert(scope, ins);
    if (scope->idx != 1 || vpk) {
        // inplace init
        return structure_init(img, scope);
    }
    return RESPOND_TRUE;
}

qresp laure_eval_array(_laure_eval_sub_args) {
    assert(e->t == let_array);
    UNPACK_CCTX(cctx);
    
    // Handle empty array []
    if (!e->ba || !e->ba->set) {
        return RESPOND_TRUE;
    }
    
    // Check if this array contains variable references
    uint element_count = laure_expression_get_count(e->ba->set);
    bool has_variable_refs = false;
    uint var_ref_count = 0;
    
    // First pass: check for variable references
    laure_expression_t *element_expr;
    EXPSET_ITER(e->ba->set, element_expr, {
        if (element_expr->t == let_name) {
            has_variable_refs = true;
            var_ref_count++;
        }
    });
    
    // If array contains variable references, set up as solution collector
    if (has_variable_refs) {
        // Create array in collection mode
        struct ArrayImage *array_img = laure_create_array_u(NULL);
        array_img->is_solution_collector = true;
        array_img->state = U; // Start uninstantiated, will collect solutions
        
        // Set up collection data
        array_img->c_data.observed_vars = laure_alloc(sizeof(Instance*) * var_ref_count);
        array_img->c_data.observed_count = var_ref_count;
        array_img->c_data.solutions = NULL;
        array_img->c_data.solution_count = 0;
        
        // Second pass: register variables for observation
        uint var_idx = 0;
        EXPSET_ITER(e->ba->set, element_expr, {
            if (element_expr->t == let_name) {
                // Look up or create variable and get its link
                ulong var_link = 0;
                Instance *var_instance = laure_scope_find_by_key_l(scope, element_expr->s, &var_link, true);
                if (!var_instance) {
                    var_instance = instance_new(element_expr->s, NULL, NULL);
                    laure_cell cell = laure_scope_insert(scope, var_instance);
                    var_link = cell.link;
                }
                
                // Add to observed names list
                array_img->c_data.observed_vars[var_idx] = var_instance;
                var_idx++;
                
                // Register array as observer of this name
                register_name_observer(var_link, array_img);
            }
        });
        
        // For now, just return success - the array is registered and will collect solutions
        // TODO: Associate array with a variable name for proper access
        
        return RESPOND_TRUE;
        
    } else {
        // Regular array evaluation (non-collecting)
        array_linked_t *first_linked = NULL;
        array_linked_t *last_linked = NULL;
        
        EXPSET_ITER(e->ba->set, element_expr, {
            // Evaluate non-variable expressions
            qresp eval_result = laure_eval(cctx, element_expr, expset);
            if (eval_result.state == q_error) {
                return eval_result;
            }
            
            // For now, simple expressions only
            RESPOND_ERROR(internal_err, element_expr, "complex array elements not yet supported");
        });
        
        return RESPOND_TRUE;
    }
}

control_ctx *control_new(laure_session_t *session, laure_scope_t* scope, qcontext* qctx, var_process_kit* vpk, void* data, bool no_ambig) {
    if (!scope) return NULL;
    
    MEMGUARD_CLEANUP_CONTEXT(cleanup);
    
    control_ctx *cctx = MEMGUARD_ALLOC(&cleanup, sizeof(control_ctx));
    if (!cctx) {
        MEMGUARD_CLEANUP(&cleanup);
        return NULL;
    }
    
    cctx->session = session;
    cctx->scope = scope;
    cctx->tmp_answer_scope = laure_scope_new(scope->glob, scope);
    if (!cctx->tmp_answer_scope) {
        MEMGUARD_CLEANUP(&cleanup);
        return NULL;
    }
    MEMGUARD_REGISTER(&cleanup, cctx->tmp_answer_scope);
    
    cctx->tmp_answer_scope->next = 0;
    cctx->qctx = qctx;
    cctx->vpk = vpk;
    cctx->data = data;
    cctx->silent = false;
    cctx->no_ambig = no_ambig;
    cctx->cut = 0;
    cctx->this_break = false;
    cctx->recursion_depth = 0;
    cctx->max_recursion_depth = 1000; // Default maximum recursion depth
    
#ifndef DISABLE_WS
    // docs: https://docs.laurelang.org/wiki/ws
    cctx->ws = laure_ws_create(NULL);
    if (!cctx->ws) {
        MEMGUARD_CLEANUP(&cleanup);
        return NULL;
    }
    MEMGUARD_REGISTER(&cleanup, cctx->ws);
#endif

    /* Transfer ownership - control_ctx now owns all allocated objects */
    MEMGUARD_TRANSFER_OWNERSHIP(&cleanup);
    MEMGUARD_CLEANUP(&cleanup);
    return cctx;
}

void control_free(control_ctx *cctx) {
    #ifndef DISABLE_WS
    laure_ws_free(cctx->ws);
    #endif
    laure_free(cctx);
}

qcontext *qcontext_new(laure_expression_set *expset) {
    qcontext *qctx = laure_alloc(sizeof(qcontext));
    qctx->constraint_mode = false;
    qctx->expset = expset;
    qctx->next = NULL;
    qctx->flagme = false;
    qctx->bag = NULL;
    return qctx;
}

/* ==========
Miscellaneous
========== */

bool laure_is_silent(control_ctx *cctx) {
    return cctx->silent;
}

void *pd_get_arg(preddata *pd, int index) {
    for (int i = 0; i < pd->argc; i++)
        if (pd->argv[i].index == index)
            return pd->argv[i].arg;
    return 0;
}

unsigned long pd_get_arg_link(preddata *pd, int index) {
    for (int i = 0; i < pd->argc; i++)
        if (pd->argv[i].index == index)
            return pd->argv[i].link_id;
    return 0;
}

string get_final_name(predfinal *pf, string shorthand) {
    if (! str_starts(shorthand, "$"))
        return shorthand;
    shorthand++;
    int id = atoi(shorthand);
    assert(id < pf->interior.argc);
    return pf->interior.argn[id];
}

string get_default_name(predfinal *pf, string final_name) {
    for (uint i = 0; i < pf->interior.argc; i++) {
        if (str_eq(pf->interior.argn[i], final_name))
            return laure_get_argn(i);
    }
    if (str_eq(pf->interior.respn, final_name))
        return laure_get_respn();
    return NULL;
}

qcontext qcontext_temp(qcontext *next, laure_expression_set *expset, Bag *bag) {
    qcontext qctx;
    qctx.bag = NULL;
    qctx.constraint_mode = false;
    qctx.expset = expset;
    qctx.flagme = false;
    qctx.next = next;
    qctx.bag = bag;
    return qctx;
}

var_process_kit vpk_create_scope_sender(scope_rec proc, void *payload) {
    var_process_kit vpk;
    vpk.mode = SENDSCOPE;
    vpk.do_process = true;
    vpk.interactive_by_name = false;
    vpk.payload = payload;
    vpk.scope_receiver = proc;
    vpk.tracked_vars = NULL;
    vpk.tracked_vars_len = 0;
    vpk.sender_receiver = NULL;
    vpk.single_var_processor = NULL;
    return vpk;
}