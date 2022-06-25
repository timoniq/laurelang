#include "laurelang.h"
#include "laureimage.h"
#include "predpub.h"

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

#define MAX_ARGS 32

char *RESPN = NULL;
char *CONTN = NULL;
char *MARKER_NODELETE = NULL;
char *DUMMY_FLAGV = NULL;
char *ARGN_BUFF[32];
bool IS_BUFFN_INITTED = 0;
qcontext *LAST_QCTX = NULL;

void laure_upd_scope(ulong link, laure_scope_t *to, laure_scope_t *from) {
    // unused
    Instance *to_ins = laure_scope_find_by_link(to, link, false);
    if (! to_ins) return;
    Instance *from_ins = laure_scope_find_by_link(from, link, false);
    if (! from_ins) return;
    bool res = image_equals(to_ins->image, from_ins->image);
    if (! res)
        printf("Error: cannot backtrack link %lu\n", link);
}

bool is_weighted_expression(laure_expression_t *exp) {
    if (exp->t == let_name || exp->t == let_command || exp->t == let_cut || exp->t == let_custom)
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
};

struct img_rec_ctx *img_rec_ctx_create(
    Instance *var, 
    control_ctx *cctx, 
    laure_expression_set *expset, 
    gen_resp (*qr_process)(qresp, struct img_rec_ctx*)
) {
    struct img_rec_ctx *ctx = malloc(sizeof(struct img_rec_ctx));
    ctx->var = var;
    ctx->cctx = cctx;
    ctx->expset = expset;
    ctx->qr_process = qr_process;
    ctx->flag = 0;
    return ctx;
}

gen_resp image_rec_default(void *image, struct img_rec_ctx *ctx) {
    void *d = ctx->var->image;
    ctx->var->image = image;
    laure_scope_t *nscope = laure_scope_create_copy(ctx->cctx, ctx->cctx->scope);
    laure_scope_t *oscope = ctx->cctx->scope;
    ctx->cctx->scope = nscope;
    qresp response = laure_start(ctx->cctx, ctx->expset);
    laure_scope_free(nscope);
    ctx->cctx->scope = oscope;
    ctx->var->image = d;
    return ctx->qr_process(response, ctx);
}

gen_resp proc_unify_response(qresp resp, struct img_rec_ctx *ctx) {
    if (resp.state == q_stop) {
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
                    bool res = image_equals(sim->image, cellptr->ptr->image);
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
            
            optimality_t a = laure_accuracy_count(current_ws);
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
        debug("sending data (mode=%s%s%s)\n", BOLD_WHITE, cctx->vpk->mode == INTERACTIVE ? "INTERACTIVE" : (cctx->vpk->mode == SENDER ? "SENDER" : "OTHER"), NO_COLOR);

        #ifndef DISABLE_WS
        if (LAURE_WS) {
            optimality_t a = laure_accuracy_count(cctx->ws);
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
        } else if (cctx->vpk->mode == SENDER) {
            response = laure_send(cctx);
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
    for (uint idx = 0; idx < MAX_ARGS; idx++) {
        char name[4];
        snprintf(name, 4, "$%u", idx);
        ARGN_BUFF[idx] = strdup(name);
    }
    IS_BUFFN_INITTED = true;
}

string laure_get_argn(uint idx) {
    assert(IS_BUFFN_INITTED);
    assert(idx < MAX_ARGS);
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
    if (str_eq(cmd, ".")) {
        return respond(q_stop, 0);
    }
    else if (strlen(cmd) == 0) {
        up;
        erase;
        showcast = crop_showcast(showcast);
        printf("%s;\n", showcast);
        free(showcast);
        return respond(q_continue, NULL);
    }
    else if (str_eq(cmd, ";")) {
        return respond(q_continue, NULL);
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
            repr = tracked->repr(tracked);
        }
        strcat(reprs, repr);
        free(repr);
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

        glob_ins->image = image_deepcopy(scope->glob, ins->image);
        
        char name[64];
        strncpy(name, vpk->tracked_vars[i], 64);

        if (str_starts(name, "$")) {
            snprintf(name, 64, "%s", ins->name);
        }

        string repr = ins->repr(ins);
        string showcast;

        if (strlen(repr) > 264) {
            printf("  %s = ", name);
            printf("%s\n", repr);
            showcast = "";
        } else {
            uint showcast_n = strlen(name) + strlen(repr) + 6;
            showcast = malloc(showcast_n + 1);
            memset(showcast, 0, showcast_n + 1);
            snprintf(showcast, showcast_n, "  %s = %s", name, repr);
        }

        free(repr);

        last_string = showcast;

        if (i + 1 != vpk->tracked_vars_len) {
            printf("%s,\n", showcast);
        } else {
            string cmd = readline(showcast);
            return check_interactive(cmd, showcast);
        }
        j++;
    }

    if (j != vpk->tracked_vars_len) {
        if (last_string != NULL) {
            up;
            erase;
            string cmd = readline(last_string);
            return check_interactive(cmd, last_string);
        }
    }

    return RESPOND_YIELD(YIELD_OK);
}

/* ===================
| Var processing kit |
=================== */

void default_var_processor(laure_scope_t *scope, string name, void* _) {
    Instance *ins = laure_scope_find_by_key(scope, name, true);
    string s = ins->repr(ins);
    printf("  %s\n", s);
    free(s);
}

int append_vars(string **vars, int vars_len, laure_expression_set *vars_exps) {
    laure_expression_t *exp = NULL;
    EXPSET_ITER(vars_exps, exp, {
        string n = exp->s;
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
                (*vars) = realloc(*vars, sizeof(void*) * (vars_len + 1));
                (*vars)[vars_len] = n;
                vars_len++;
            }
        }
    });
    return vars_len;
}

var_process_kit *laure_vpk_create(laure_expression_set *expset) {
    string *vars = malloc(sizeof(string));
    int vars_len = 0;

    laure_expression_t *exp = NULL;
    EXPSET_ITER(expset, exp, {
        laure_expression_set *vars_exps = laure_get_all_vars_in(exp, NULL);
        vars_len = append_vars(&vars, vars_len, vars_exps);
    });

    var_process_kit *vpk = malloc(sizeof(var_process_kit));
    vpk->do_process = true;
    vpk->interactive_by_name = false;
    vpk->mode = INTERACTIVE;
    vpk->payload = NULL;
    vpk->single_var_processor = default_var_processor;
    vpk->tracked_vars = vars;
    vpk->tracked_vars_len = vars_len;
    return vpk;
}

void laure_vpk_free(var_process_kit *vpk) {
    free(vpk->tracked_vars);
    free(vpk);
}

#define _laure_eval_sub_args control_ctx *cctx, laure_expression_t *e, laure_expression_set *expset
qresp laure_eval_assert(_laure_eval_sub_args);
qresp laure_eval_image(_laure_eval_sub_args);
qresp laure_eval_name(_laure_eval_sub_args);
qresp laure_eval_var(_laure_eval_sub_args);
qresp laure_eval_unify(_laure_eval_sub_args);
qresp laure_eval_pred_call(_laure_eval_sub_args);
qresp laure_eval_callable_decl(_laure_eval_sub_args);
qresp laure_eval_quant(_laure_eval_sub_args);
qresp laure_eval_imply(_laure_eval_sub_args);
qresp laure_eval_choice(_laure_eval_sub_args);
qresp laure_eval_cut(_laure_eval_sub_args);
qresp laure_eval_set(_laure_eval_sub_args);
qresp laure_eval_command(_laure_eval_sub_args);

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
        case let_name: {
            return laure_eval_name(cctx, e, expset);
        }
        #ifndef FORBID_NAME_EVAL
        case let_var: {
            return laure_eval_var(cctx, e, expset);
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
        #ifndef FORBID_COMMAND
        case let_command: {
            return laure_eval_command(cctx, e, expset);
        }
        #endif
        default: {
            RESPOND_ERROR(internal_err, e, "can't evaluate {%s} in MAIN context", EXPT_NAMES[e->t]);
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
    
    if (lvar->t == let_var && rvar->t == let_var) {
        // When var asserted to var
        // check whether they exist or not

        Instance *lvar_ins = laure_scope_find_by_key(scope, lvar->s, true);
        Instance *rvar_ins = laure_scope_find_by_key(scope, rvar->s, true);

        if (lvar_ins && rvar_ins) {
            // check if they are instantiated
            bool li = instantiated(lvar_ins);
            bool ri = instantiated(rvar_ins);

            if (li && ri) {
                return image_equals(lvar_ins->image, rvar_ins->image) ? RESPOND_TRUE : RESPOND_FALSE;
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

                if (h.t != oldh.t) return RESPOND_FALSE;

                return image_equals(from->image, to->image) ? RESPOND_TRUE : RESPOND_FALSE;
            } else {
                // choicepoint is created
                // due to ambiguation
                // **intersection**
                Instance *var1;
                Instance *var2;
                if (lvar_ins->locked && !rvar_ins->locked) {
                    var1 = lvar_ins;
                    var2 = rvar_ins;
                } else if (!lvar_ins->locked || !rvar_ins->locked) {
                    var1 = rvar_ins;
                    var2 = lvar_ins;
                } else {
                    RESPOND_ERROR(
                        access_err,
                        e,
                        "both %s and %s are locked variables, so no ambiguation may be created", 
                        lvar_ins->name, rvar_ins->name
                    );
                }

                bool eqr = image_equals(var1->image, var2->image);
                if (! eqr) {
                    return RESPOND_FALSE;
                }

                // context
                struct img_rec_ctx *ctx = img_rec_ctx_create(var2, cctx, expset, qr_process_default);
                // -------
                gen_resp gr = image_generate(scope, var1->image, image_rec_default, ctx);
                free(ctx);

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

            Instance *nvar = instance_deepcopy(scope, nvar_name, srcvar);
            nvar->locked = false;

            laure_scope_insert(scope, nvar);

            return RESPOND_TRUE;
        } else {
            RESPOND_ERROR(undefined_err, e, "both variables %s and %s are undefined and cannot be asserted", lvar->s, rvar->s);
        }
    } else if (lvar->t == let_var || rvar->t == let_var) {
        string vname = lvar->t == let_var ? lvar->s : rvar->s;
        laure_expression_t *rexpression = lvar->t == let_var ? rvar : lvar;
        Instance *to = laure_scope_find_by_key(scope, vname, true);

        if (!to)
            RESPOND_ERROR(undefined_err, e, "variable %s", vname);
        
        if (to->locked)
            RESPOND_ERROR(access_err, e, "%s is locked", vname);
        else {
            // Expression is sent to image translator
            struct ImageHead head = read_head(to->image);
            if (! head.translator) {
                RESPOND_ERROR(internal_err, e, "%s is not assignable. Cannot assign to `%s`", vname, rexpression->s);
            }
            bool result = head.translator->invoke(rexpression, to->image, scope);
            if (!result) return RESPOND_FALSE;
            return RESPOND_TRUE;
        }
    } else
        RESPOND_ERROR(instance_err, e, "can't assert %s with %s", lvar->s, rvar->s);
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
        if (exp1->t != let_var) RESPOND_ERROR(syntaxic_err, e, "left value imaged to atomic set %s should be a variable", exp2->s);
        Instance *ins = laure_scope_find_by_key(scope, exp1->s, true);
        if (ins) {
            if (read_head(ins->image).t != ATOM) return RESPOND_FALSE;
        }
        multiplicity *mult = multiplicity_create();
        laure_expression_t *ptr = NULL;
        EXPSET_ITER(exp2->ba->set, ptr, {
            char name[ATOM_LEN_MAX];
            write_atom_name(ptr->s, name);
            multiplicity_insert(mult, strdup(name));
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

    if (exp1->t == let_var && exp2->t == let_var) {

        Instance *var1 = laure_scope_find_by_key(scope, exp1->s, true);
        Instance *var2 = laure_scope_find_by_key(scope, exp2->s, true);

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
            string    new_var_name;
            uint nesting;
            uint secondary_nesting;

            if (var1) {
                from = var1;
                new_var_name = exp2->s;
                nesting = exp1->flag;
                secondary_nesting = exp2->flag;
            } else {
                from = var2;
                new_var_name = exp1->s;
                nesting = exp2->flag;
                secondary_nesting = exp1->flag;
            }

            absorb(nesting, secondary_nesting);

            Instance *ins = instance_deepcopy(scope, new_var_name, from);

            if (nesting) {
                while (nesting) {
                    void *img = laure_create_array_u(ins);
                    ins = instance_new(MOCK_NAME, NULL, img);
                    ins->repr = array_repr;
                    nesting--;
                }
                ins->name = strdup(new_var_name);
                ins->repr = array_repr;
            }

            laure_scope_insert(scope, ins);
            return RESPOND_TRUE;
        } else
            RESPOND_ERROR(undefined_err, e, "both variables %s and %s are unknown", exp1->s, exp2->s);
        
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
=-------= */
qresp laure_eval_name(_laure_eval_sub_args) {
    assert(e->t == let_name);
    UNPACK_CCTX(cctx);
    laure_expression_t *v1_exp = e->ba->set->expression;
    laure_expression_t *v2_exp = e->ba->set->next->expression;

    ulong link1[1], link2[1];

    Instance *v1 = laure_scope_find_by_key_l(scope, v1_exp->s, link1, false);
    Instance *v2 = laure_scope_find_by_key_l(scope, v2_exp->s, link2, false);

    if (!v1 && !v2) {
        return respond(q_error, "naming impossible, neither left nor right is known");
    } else if (v1 && v2) {
        if (v1 == v2) return RESPOND_TRUE;
        else if (instantiated(v1) || instantiated(v2)) {
            return image_equals(v1->image, v2->image) ? RESPOND_TRUE : RESPOND_FALSE;
        } else {
            laure_scope_change_link_by_key(scope, v2_exp->s, link1[0], false);
            return RESPOND_TRUE;
        }
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
        RESPOND_ERROR(undefined_err, e, "variable %s", e->s);
    else if (to_unif->locked)
        RESPOND_ERROR(access_err, e, "%s is locked", e->s);
    struct img_rec_ctx *ctx = img_rec_ctx_create(to_unif, cctx, expset, proc_unify_response);
    gen_resp gr = image_generate(scope, to_unif->image, image_rec_default, ctx);
    if (! ctx->flag) return respond(q_yield, YIELD_FAIL);
    free(ctx);
    if (gr.qr.state == q_stop) {
        return gr.qr;
    }
    return respond(q_yield, gr.qr.payload);
}

/* =-------=
Shows variable.
=-------= */
qresp laure_eval_var(_laure_eval_sub_args) {
    assert(e->t == let_var);
    UNPACK_CCTX(cctx);

    if (!vpk) return RESPOND_TRUE;

    string vname = e->s;
    if (e->flag > 0)
        RESPOND_ERROR(instance_err, e, "cannot evaluate nesting of %s, use infer op.", vname);
    
    Instance *var = laure_scope_find_by_key(scope, vname, true);
    if (!var) RESPOND_ERROR(undefined_err, e, "variable %s", vname);

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
        return NULL;
    } else if (head.translator->identificator == STRING_TRANSLATOR->identificator) {
        return laure_scope_find_by_key(scope->glob, "string", false);
    }
    return NULL;
}

Instance *prepare_T_instance(laure_scope_t *scope, Instance *resolved, uint nesting) {
    Instance *prepared = laure_unwrap_nestings(resolved, nesting);
    if (! prepared) {
        printf("Unable to unwrap nestings to perform generic preparation\n");
        return NULL;
    }
    prepared = get_derived_instance(scope, prepared);
    if (! prepared) {
        printf("Unable to find what instance datatype was derived from; notice that atom datatype cannot be used in generic\n");
        return NULL;
    }
    prepared = instance_shallow_copy(prepared);
    return prepared;
}

Instance *resolve_generic_T(
    struct PredicateHeaderImage header,
    laure_expression_set *set,
    laure_scope_t *scope
) {
    if (header.resp && header.resp->t == td_generic) {
        laure_expression_t *rexp = laure_expression_set_get_by_idx(set, header.args->length);
        if (rexp && rexp->t == let_var) {
            Instance *resolved = laure_scope_find_by_key(scope, rexp->s, false);
            if (resolved) {
                uint nesting = header.response_nesting;
                debug("T resolved by response\n");
                Instance *final =  prepare_T_instance(scope, resolved, nesting);
                debug("Resolved instance: %s\n", final->repr(final));
                return final;
            }
        }
    }
    for (uint i = 0; i < header.args->length; i++) {
        if (header.args->data[i].t == td_generic) {
            laure_expression_t *exp = laure_expression_set_get_by_idx(set, i);
            if (exp->t == let_var) {
                Instance *resolved = laure_scope_find_by_key(scope, exp->s, false);
                if (resolved) {
                    uint nesting = header.nestings[i];
                    debug("T resolved by argument %u\n", i);
                    Instance *final = prepare_T_instance(scope, resolved, nesting);
                    debug("Resolved instance: %s\n", final->repr(final));
                    return final;
                }
            }
        }
    }
    return NULL;
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

    Instance *T,
    uint nesting
) {
    switch (exp->t) {
        case let_var: {
            // name of var in prev_scope
            string vname = exp->s;
            if (isanonvar(vname))
                vname = laure_scope_generate_unique_name();
            
            ulong link[1];
            link[0] = 0;
            Instance *arg = laure_scope_find_by_key_l(prev_scope, vname, link, true);

            if (arg && arg->locked) {
                return_str_fmt("%s is locked", arg->name);
            } else if (! arg) {
                if (hint_opt) {
                    Instance *hint;
                    if (hint_opt->t == td_generic) {
                        assert(T);
                        hint = get_nested_instance(T, nesting, prev_scope);
                    } else {
                        hint = hint_opt->instance;
                    }
                    arg = instance_deepcopy(prev_scope, argn, hint);
                } else {
                    return_str_fmt("specification of %s is needed", vname);
                }
                ulong preset_link = 0;
                if (prev_scope->idx == 1) {
                    ulong l[1];
                    Instance *glob = laure_scope_find_by_key_l(cctx->tmp_answer_scope, vname, l, false);
                    if (! glob) {
                        Instance *nins = instance_deepcopy(cctx->tmp_answer_scope, vname, arg);
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

                Instance *prev_ins = instance_deepcopy(prev_scope, vname, arg);

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
                Instance *existing_same = new_scope ? laure_scope_find_by_link(new_scope, *link, false) : NULL;
                if (existing_same && recorder)
                    arg = instance_new(argn, MARKER_NODELETE, existing_same->image);
                else if (create_copy && recorder)
                    arg = instance_deepcopy(new_scope, argn, arg);
            }
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
                    Instance *nins = instance_deepcopy(cctx->tmp_answer_scope, vname, arg);
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
                assert(T);
                hint = get_nested_instance(T, nesting, prev_scope);;
            } else {
                hint = hint_opt->instance;
            }
            Instance *arg = instance_deepcopy(prev_scope, argn, hint);
            
            if (! arg->image)
                return_str_fmt("specification of %s is needed", argn);
            bool result = read_head(arg->image).translator->invoke(exp, arg->image, prev_scope);

            if (! result) {
                image_free(arg->image);
                free(arg);
                return ARGPROC_RET_FALSE;
            }
            if (recorder)
                recorder(arg, laure_scope_generate_link(), ctx);
            else {
                image_free(arg->image);
                free(arg);
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

/* =-------=
Predicate/constraint call.
=-------= */
#define pred_call_cleanup do {laure_scope_free(init_scope); laure_scope_free(prev);} while (0)
qresp laure_eval_pred_call(_laure_eval_sub_args) {
    assert(e->t == let_pred_call);
    UNPACK_CCTX(cctx);
    string predicate_name = e->s;
    if (str_eq(e->s, RECURSIVE_AUTONAME)) {
        if (! scope->owner) 
            RESPOND_ERROR(syntaxic_err, e, "autoname can only be used inside other predicates%s", "");
        predicate_name = scope->owner;
    }
    Instance *predicate_ins = laure_scope_find_by_key(scope, predicate_name, true);
    if (! predicate_ins)
        RESPOND_ERROR(undefined_err, e, "predicate %s", predicate_name);
    enum ImageT call_t = read_head(predicate_ins->image).t;
    if (! (call_t == PREDICATE_FACT || call_t == CONSTRAINT_FACT))
        RESPOND_ERROR(instance_err, e, "%s is neither predicate nor constraint", predicate_name);
    bool is_constraint = (call_t == CONSTRAINT_FACT);
    bool tfc = is_constraint && !qctx->constraint_mode;

    struct PredicateImage *pred_img = (struct PredicateImage*) predicate_ins->image;
    if (pred_img->is_primitive)
        RESPOND_ERROR(too_broad_err, e, "primitive (%s) call is prohibited before instantiation; unify", predicate_name);
    if (is_constraint)
        qctx->constraint_mode = true;
    if (pred_img->variations->finals == NULL) {
        // first call constructs finals
        void **pfs = malloc(sizeof(void*) * pred_img->variations->len);
        for (uint i = 0; i < pred_img->variations->len; i++) {
            struct PredicateImageVariation pv = pred_img->variations->set[i];
            predfinal *pf = get_pred_final(pv);
            pfs[i] = pf;
        }
        pred_img->variations->finals = (predfinal**)pfs;
    }

    bool need_more = false;
    bool found = false;

    laure_scope_t *init_scope = laure_scope_create_copy(cctx, scope);
    for (uint variation_idx = 0; variation_idx < pred_img->variations->len; variation_idx++) {
        predfinal *pf = pred_img->variations->finals[variation_idx];
        laure_scope_t *prev = laure_scope_create_copy(cctx, init_scope);
        qresp resp;
        bool do_continue = true;
        ulong cut_store = cctx->cut;
        bool cut_case = false;

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
                    actx, false, NULL, 0);

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
                    pd->resp = instance_deepcopy(prev, MOCK_NAME, hint);
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
                        actx, false, NULL, 0);
                    
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
            preddata_free(pd);

            if (resp.state == q_stop) return resp;

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
                continue;
            }

            laure_expression_set *full = qctx->expset;
            qctx->expset = expset;
            cctx->scope = prev;
            resp = laure_start(cctx, expset);
            cctx->scope = scope;
            qctx->expset = full;
        } else {

            if (e->ba->body_len != pf->interior.argc) {
                laure_scope_free(init_scope);
                RESPOND_ERROR(signature_err, e, "predicate %s got %d args, expected %d", predicate_name, e->ba->body_len, pf->interior.argc);
            }

            laure_expression_t *exp = pred_img->variations->set[variation_idx].exp;
            cut_case = PREDFLAG_IS_CUT(exp->flag);

            laure_scope_t *nscope = laure_scope_new(scope->glob, prev);
            nscope->owner = predicate_ins->name;

            struct arg_rec_ctx actx[1];
            actx->new_scope = nscope;

            Instance *T = NULL;
            if (! laure_typeset_all_instances(pred_img->header.args)) {
                if (e->docstring && strlen(e->docstring)) {
                    // generic was set up manually
                    uint nesting = e->flag;
                    T = laure_scope_find_by_key(scope->glob, e->docstring, true);
                    if (T) {
                        T = get_nested_instance(T, nesting, scope->glob);
                        T = instance_shallow_copy(T);
                    }
                    else
                        RESPOND_ERROR(undefined_err, e, "instance %s is undefined, can't resolve the generic type", e->docstring);
                }
                if (! T) {
                    T = resolve_generic_T(pred_img->header, e->ba->set, scope);
                    if (! T) RESPOND_ERROR(signature_err, e, "unable to resolve generic datatype%s; add explicit cast or add hint to resolve", "");
                }
            }

            laure_expression_set *arg_l = e->ba->set;
            for (uint idx = 0; idx < e->ba->body_len; idx++) {
                actx->idx_pd = idx;
                laure_expression_t *argexp = arg_l->expression;

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
                    actx, true, T, pred_img->header.nestings[idx]);

                ARGPROC_RES_(
                    res, 
                    {}, 
                    {pred_call_cleanup; return respond(q_error, err);}, 
                    {pred_call_cleanup; return RESPOND_FALSE;},
                    err);

                arg_l = arg_l->next;
            }

            if (pf->interior.respn) {
                if (! arg_l) {
                    Instance *hint = NULL;
                    if (pred_img->header.resp) {
                        if (pred_img->header.resp->t == td_generic) {
                            hint = get_nested_instance(T, pred_img->header.response_nesting, scope);
                        } else {
                            hint = pred_img->header.resp->instance;
                        }
                    }
                    if (! hint)
                        RESPOND_ERROR(signature_err, e, "specification of %s's response is needed", predicate_name);
                    if (! str_eq(pf->interior.respn, "_")) {
                        Instance *rins = instance_deepcopy(nscope, laure_get_respn(), hint);
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
                        actx, true, T, pred_img->header.response_nesting);
                    
                    ARGPROC_RES_(
                        res,
                        {},
                        {pred_call_cleanup; return respond(q_error, err);},
                        {pred_call_cleanup; return RESPOND_FALSE;},
                        err);
                }
            }

            if (T) free(T);
            
            laure_expression_set *body = pf->interior.body;
            body = laure_expression_compose(body);
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
            break;
        } else if (cctx->cut != 0 && cctx->cut - 1 > scope->idx) {
            debug("stop cutting on predicate %s\n", predicate_name);
            cctx->cut = 0;
        } else if (resp.state == q_true || resp.state == q_continue) {
            found = true;
            if (cut_case) break;
        } else if (resp.state == q_yield) {
            if (resp.payload == YIELD_OK) {
                found = true;
                if (cut_case) break;
            }
        }
        // laure_scope_free(prev);
    }
    // laure_scope_free(init_scope);
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

    Instance *ins = laure_scope_find_by_key(scope, vname, false);
    if (! ins)
        RESPOND_ERROR(undefined_err, e, "variable %s", vname);
    
    bool dp = cctx->vpk->do_process;
    
    qcontext nqctx[1];
    nqctx->constraint_mode = qctx->constraint_mode;
    nqctx->expset = set;
    nqctx->next = NULL;

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
            gen_resp gr = image_generate(scope, ins->image, image_rec_default, ctx);
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
            gen_resp gr = image_generate(scope, ins->image, image_rec_default, ctx);
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
    current->constraint_mode = qctx->constraint_mode;
    current->expset = expset;
    current->next = qctx->next;

    qcontext if_qctx[1];
    if_qctx->constraint_mode = true;
    if_qctx->expset = implies_for;
    if_qctx->next = current;

    qcontext fact_qctx[1];
    fact_qctx->constraint_mode = true;
    fact_qctx->expset = fact_set;
    fact_qctx->next = if_qctx;
    fact_qctx->flagme = false;

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
    laure_expression_set *set = e->ba->set;
    bool found = false;
    while (set) {
        laure_expression_set *choice = set->expression->ba->set;
        laure_scope_t *nscope = laure_scope_create_copy(cctx, scope);
        nscope->repeat++;

        laure_expression_set *old = qctx->expset;

        qcontext nqctx[1];
        nqctx->constraint_mode = qctx->constraint_mode;
        nqctx->expset = choice;
        nqctx->next = qctx;
        qctx->expset = expset;

        cctx->qctx = nqctx;
        cctx->scope = nscope;
        qresp response = laure_start(cctx, choice);
        laure_scope_free(nscope);

        if (response.state == q_error)
            return response;
        
        else if (response.state == q_true || (response.state == q_yield && response.payload == YIELD_OK))
            found = true;
        
        cctx->scope = scope;
        cctx->qctx = qctx;
        qctx->expset = old;
        set = set->next;
    }
    return RESPOND_YIELD(YIELD_OK);
}

/* =-------=
Cutting tree
=-------= */
qresp laure_eval_cut(_laure_eval_sub_args) {
    assert(e->t == let_cut);
    UNPACK_CCTX(cctx);
    cctx->cut = scope->idx;
    return respond(q_true, 0);
}

string get_work_dir_path(string f_addr) {
    string naddr = strdup(f_addr);
    strrev_via_swap(naddr);
    while (naddr[0] != '/') naddr++;
    strrev_via_swap(naddr);
    return naddr;
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
        nqctx->constraint_mode = qctx->constraint_mode;
        nqctx->expset = e->ba->set;
        nqctx->next = NULL;

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
        nqctx->constraint_mode = qctx->constraint_mode;
        nqctx->expset = e->ba->set;
        nqctx->flagme = false;
        nqctx->next = qctx;

        cctx->qctx = nqctx;

        qresp response = laure_start(cctx, cctx->qctx->expset);

        qctx->expset = old;
        cctx->qctx = qctx;
        return response;
    }
}

string string_clean(string s);

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
            if (! v) {
                v = DUMMY_FLAGV;
            }
            add_dflag(n, v);
            return RESPOND_TRUE;
        }
        case command_use:
        case command_useso: {
            string_linked *linked = string_split(e->s, ',');
            bool failed[1];
            failed[0] = false;

            while (linked) {
                string name = linked->s;
                name = string_clean(name);
                if (name[0] == '"') name++;
                if (lastc(name) == '"') lastc(name) = 0;

                string n;

                if (str_starts(name, "/")) {
                    n = strdup(name);
                } else if (str_starts(name, "@/")) {
                    name++;
                    n = malloc(strlen(lib_path) + strlen(name) + 1);
                    strcpy(n, lib_path);
                    strcat(n, name);
                } else {
                    string wdir = get_work_dir_path(LAURE_CURRENT_ADDRESS ? LAURE_CURRENT_ADDRESS : "./");
                    n = malloc(strlen(wdir) + strlen(name) + 1);
                    strcpy(n, wdir);
                    strcat(n, name);
                }

                string path_ = malloc(PATH_MAX);
                memset(path_, 0, PATH_MAX);
                realpath(n, path_);

                if (e->flag == command_useso) {
                    bool result = laure_load_shared(cctx->session, path_);
                    free(path_);
                } else {
                    FILE *f = fopen(path_, "r");
                    if (!f) {
                        printf("%s\n", path_);
                        free(path_);
                        RESPOND_ERROR(undefined_err, e, "failed to find file %s", path_);
                    }
                    fclose(f);
                    laure_consult_recursive(cctx->session, path_, (int*)failed);
                }
                linked = linked->next;
            }

            return failed[0] ? RESPOND_FALSE : RESPOND_TRUE;
        }
    }
    return RESPOND_TRUE;
}

control_ctx *control_new(laure_session_t *session, laure_scope_t* scope, qcontext* qctx, var_process_kit* vpk, void* data, bool no_ambig) {
    control_ctx *cctx = malloc(sizeof(control_ctx));
    cctx->session = session;
    cctx->scope = scope;
    cctx->tmp_answer_scope = laure_scope_new(scope->glob, scope);
    cctx->tmp_answer_scope->next = 0;
    cctx->qctx = qctx;
    cctx->vpk = vpk;
    cctx->data = data;
    cctx->silent = false;
    cctx->no_ambig = no_ambig;
    cctx->cut = 0;
#ifndef DISABLE_WS
    // docs: https://docs.laurelang.org/wiki/ws
    cctx->ws = laure_ws_create(NULL);
#endif
    return cctx;
}

void control_free(control_ctx *cctx) {
    #ifndef DISABLE_WS
    laure_ws_free(cctx->ws);
    #endif
    free(cctx);
}

qcontext *qcontext_new(laure_expression_set *expset) {
    qcontext *qctx = malloc(sizeof(qcontext));
    qctx->constraint_mode = false;
    qctx->expset = expset;
    qctx->next = NULL;
    qctx->flagme = false;
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
    return NULL;
}