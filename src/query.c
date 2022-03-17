#include "laurelang.h"
#include "laureimage.h"
#include "predpub.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include "readline/readline.h"

#define RESPOND_OK laure_eval(cctx, expression_set)
#define is_global(stack) (stack->global == stack)

#define ANONVAR_NAME "_"

#define __query_free_scopes_nqctx do { \
        laure_stack_free(prev_stack); \
        laure_stack_free(new_stack); \
        free(nqctx); \
    } while (0)

#define __free_initial_and_quit(__state, __err) do {\
            laure_stack_free(initial_stack); \
            return respond(__state, __err); \
        } while (0)

#define __must_stop(__state, __err, __no_ambig) ((__state == q_false || (__state == q_yield && __err == 0)) && __no_ambig)
#define isanonvar(__name) (str_eq(__name, ANONVAR_NAME))

typedef struct {
    control_ctx *cctx;
    laure_expression_set *expset;
    long lid;
    bool force;
} igctx;

struct ImgRecCtx {
    Instance *var;
    control_ctx *cctx;
    laure_expression_set *expset;
};

qcontext *qcontext_new(laure_expression_set *expset) {
    qcontext *qctx = malloc(sizeof(qcontext));
    qctx->constraint_mode = false;
    qctx->expset = expset;
    qctx->next = NULL;
    qctx->mark = false;
    qctx->cut = false;
    return qctx;
}

qcontext *qcontext_new_shifted(qcontext *qctx, laure_expression_set *expression_set) {
    qcontext *nextqctx = qcontext_new(qctx->next);
    nextqctx->constraint_mode = qctx->constraint_mode;
    nextqctx->expset = expression_set;
    nextqctx->next = qctx->next;
    return nextqctx;
}

control_ctx *control_new(laure_stack_t* stack, qcontext* qctx, var_process_kit* vpk, void* data, bool no_ambig) {
    control_ctx *cctx = malloc(sizeof(control_ctx));
    cctx->stack = stack;
    cctx->qctx = qctx;
    cctx->vpk = vpk;
    cctx->data = data;
    cctx->silent = false;
    cctx->no_ambig = no_ambig;
    return cctx;
}

void *pd_get_arg(preddata *pd, int index) {
    for (int i = 0; i < pd->argc; i++) {
        if (pd->argv[i].index == index) {
            return pd->argv[i].arg;
        }
    }
    return NULL;
}

long pd_get_arg_link(preddata *pd, int index) {
    for (int i = 0; i < pd->argc; i++) {
        if (pd->argv[i].index == index) {
            return pd->argv[i].link_id;
        }
    }
    return 0;
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

void default_var_processor(laure_stack_t *stack, string name, void* _) {
    Instance *ins = laure_stack_get(stack, name);
    string s = ins->repr(ins);
    printf("  %s\n", s);
    free(s);
}

bool laure_is_silent(void *cctx) {
    return ((control_ctx*)cctx)->silent;
}

control_ctx *laure_control_ctx_get(laure_session_t *session, laure_expression_set *expset) {
    qcontext *qctx = qcontext_new(expset);

    // Getting vars to track
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

    return control_new(session->stack, qctx, vpk, NULL, false);
}

gen_resp image_generator_rec(void *img, igctx *ctx) {
    laure_stack_t *stack = laure_stack_clone(ctx->cctx->stack, true);

    Cell ncell = laure_stack_get_cell_by_lid(stack, ctx->lid, false);
    ncell.instance->image = img;

    control_ctx *cctx_new = create_control_ctx(stack, ctx->cctx->qctx, ctx->cctx->vpk, (void*)0x1, ctx->cctx->no_ambig);

    qresp r = laure_eval(cctx_new, ctx->expset);

    // free(cctx_new);

    if (r.state != q_true) {

        if ((ctx->force && r.state == q_stop) || ctx->cctx->no_ambig) {
            gen_resp gr = {0, r};
            return gr;
        }
        gen_resp gr = {r.state == q_error || r.state == q_stop ? 0 : 1, r};
        return gr;
    }

    gen_resp gr = {1, respond(q_true, NULL)};
    return gr;
}

qresp process_choice(
    uint count, 
    uint i, 
    laure_stack_t *nstack, 
    laure_expression_set *nexpset, 
    control_ctx *cctx,
    laure_expression_set *expset
) {
    qcontext *next_qctx = qcontext_new_shifted(cctx->qctx, expset);
    nstack->repeat = 1;
    
    qcontext *nqctx = qcontext_new(nexpset);
    nqctx->next     = next_qctx;
    nqctx->expset = nexpset;

    control_ctx *ncctx = control_new(nstack, nqctx, cctx->vpk, 0x1, cctx->no_ambig);
    qresp choice_qr = laure_eval(ncctx, nexpset);

    free(ncctx);
    free(nqctx);
    free(next_qctx);

    return choice_qr;
}

gen_resp image_rec1(void *image, void *ctx_r) {
    struct ImgRecCtx *ctx = (struct ImgRecCtx*)ctx_r;
    ctx->var->image = image;

    laure_stack_t *stack = laure_stack_clone(ctx->cctx->stack, true);

    control_ctx *cctx_new = create_control_ctx(
        stack, 
        ctx->cctx->qctx, 
        ctx->cctx->vpk, 
        ctx->cctx->data,
        ctx->cctx->no_ambig
    );

    qresp qr = laure_eval(cctx_new, ctx->expset);

    free(cctx_new);
    // laure_stack_free(stack);

    if (qr.error != NULL || qr.state == q_stop || qr.state == q_false) {
        gen_resp gr = {0, qr};
        return gr;
    }

    gen_resp gr = {1};
    return gr;
}

#ifdef FEATURE_LINKED_SCOPE

void instant_referred(laure_stack_t *stack, Cell cell1) {
    Cell cell2;

    STACK_ITER(stack, cell2, {
        if (
            (cell1.instance != cell2.instance)
            && (cell1.link_id == cell2.link_id)
        ) {
            if (!instantiated(cell1.instance)) {
                cell1.instance->image = image_deepcopy(stack, cell2.instance->image);
            } else if (!instantiated(cell2.instance)) {
                cell2.instance->image = image_deepcopy(stack, cell1.instance->image);
            }
        }
    }, false);
}

bool check_eq_eval(laure_stack_t *stack, Cell cell1) {
    Cell cell2;
    STACK_ITER(stack, cell2, {
        if (cell1.link_id == cell2.link_id && cell1.instance != cell2.instance) {
            if (! img_equals(cell1.instance->image, cell2.instance->image)) {
                return false;
            }
        }
    }, false);
    return true;
}


void merge_eval(laure_stack_t *stack_cur, laure_stack_t *stack_next) {
    if (!stack_next) return;
    assert(stack_cur);

    laure_stack_t *scope = stack_cur;
    do {
        Cell cell;

        STACK_ITER(scope, cell, {
            Cell c = laure_stack_get_cell_by_lid(stack_next, cell.link_id, false);
            if (c.instance) {
                c.instance->image = image_deepcopy(stack_next, cell.instance->image);
            }
        }, false);

        scope = scope->main;
    } while (scope && scope->is_temp);
}

#endif

gen_resp quantif_all_gen(void *image, void *ctx_r) {
    igctx *ctx = (igctx*)ctx_r;
    laure_stack_t *stack = laure_stack_clone(ctx->cctx->stack, false);
    Cell cell = laure_stack_get_cell_by_lid(stack, ctx->lid, true);

    cell.instance->image = image;
    control_ctx *ncctx = create_control_ctx(stack, ctx->cctx->qctx, ctx->cctx->vpk, ctx->cctx->data, ctx->cctx->no_ambig);

    qresp qr = laure_eval(
        ncctx,
        ncctx->qctx->expset
    );

    if (qr.state == q_false || qr.error != NULL) {
        gen_resp gr = {0, qr};
        return gr;
    }

    gen_resp gr = {1, respond(q_true, NULL)};
    return gr;
}

gen_resp quantif_exists_gen(void *image, void *ctx_r) {
    igctx *ctx = (igctx*)ctx_r;
    laure_stack_t *stack = laure_stack_clone(ctx->cctx->stack, false);
    Cell cell = laure_stack_get_cell_by_lid(stack, ctx->lid, true);

    cell.instance->image = image;
    control_ctx *ncctx = create_control_ctx(stack, ctx->cctx->qctx, ctx->cctx->vpk, ctx->cctx->data, ctx->cctx->no_ambig);

    qresp qr = laure_eval(
        ncctx,
        ncctx->qctx->expset
    );

    if (qr.state == q_true || qr.state == q_true_s || qr.error != NULL) {
        gen_resp gr = {0, qr};
        return gr;
    }

    gen_resp gr = {1, respond(q_false, NULL)};
    return gr;
}

#define up printf("\033[A")
#define down printf("\n")
#define erase printf("\33[2K\r")

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
        return respond(q_stop, "");
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

qresp laure_showcast(laure_stack_t *stack, var_process_kit *vpk) {

    string last_string = NULL;
    int j = 0;

    for (int i = 0; i < vpk->tracked_vars_len; i++) {
        Instance *ins = laure_stack_get(stack, vpk->tracked_vars[i]);
        if (!ins) continue;
        
        if (stack != stack->global) {
            Cell cell = laure_stack_get_cell(stack->global, ins->name);
            if (!cell.instance) {
                Cell ncell;
                ncell.link_id = laure_stack_get_uid(stack->global);
                ncell.instance = instance_deepcopy(stack->global, strdup(ins->name), ins);
                laure_stack_insert(stack->global, ncell);
            } else {
                cell.instance->image = image_deepcopy(stack, ins->image);
            }
        }
        
        char name[64];
        strncpy(name, ins->name, 64);

        if (str_starts(name, "$")) {
            string doc = instance_get_doc(ins);
            if (doc && strlen(doc)) {
                snprintf(name, 64, "%s %s[%s]%s", ins->name, GRAY_COLOR, doc, NO_COLOR);
            }
        }

        string repr = ins->repr(ins);

        string showcast;

        if (strlen(repr) > 264) {
            printf("  %s = ", name, repr);
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

        // printf("  %s = %s,\n", vpk->tracked_vars[i], repr);
        // free(repr);
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

    return respond(q_true, 0);
}

qresp laure_send(laure_stack_t *stack, var_process_kit *vpk) {
    #ifdef LAURE_SENDER_USE_JSON_SENDER
    printf("not implemented json sender\n");
    exit(-1);
    #else
    char reprs[1024] = {0};
    strcpy(reprs, "=");
    for (int i = 0; i < vpk->tracked_vars_len; i++) {
        string repr;
        Instance *tracked = laure_stack_get(stack, vpk->tracked_vars[i]);
        if (!tracked) repr = strdup("UNKNOWN");
        else {
            repr = tracked->repr(tracked);
        }
        strcat(reprs, repr);
        free(repr);
        if (i != vpk->tracked_vars_len - 1) strcat(reprs, ";");
    }
    bool result = vpk->sender_receiver(strdup(reprs), vpk->payload);
    return respond(result ? q_true : q_stop, result ? 0 : "");
    #endif
}

string array_repr(Instance *ins);

qresp laure_eval(control_ctx *cctx, laure_expression_set *expression_set) {

    if (LAURE_TIMEOUT != 0) {
        double elapsed_sec = ((double)(clock() - LAURE_CLOCK)) / CLOCKS_PER_SEC;
        if (elapsed_sec >= LAURE_TIMEOUT) {
            // printf("%s\\%s TIMEOUT. elapsed: %fs\n", RED_COLOR, NO_COLOR, elapsed_sec);
            RESPOND_ERROR("TIMEOUT. elapsed: %fs", elapsed_sec);
        }
    }

    laure_stack_t *stack = cctx->stack;
    var_process_kit *vpk = cctx->vpk;
    qcontext *qctx = cctx->qctx;

    if (laure_expression_get_count(expression_set) == 0) {
        if ((!qctx || qctx->next == NULL) || (stack->next == NULL && !stack->repeat)) {
            if (!vpk || cctx->silent || cctx->data == 0x2) return respond(q_true, 0);
            if (qctx) qctx->mark = true;
            qresp qr;
            if (vpk->mode == INTERACTIVE) {
                qr = laure_showcast(stack, vpk);
            } else if (vpk->mode == SENDER) {
                qr = laure_send(stack, vpk);
            }
            return qr;
        } else {
            laure_stack_t *nstack;

            if (stack->repeat > 0) {
                stack->repeat--;
                nstack = stack;
            } else if (stack != stack->next) {

                // instantiating referred instances n^2

                #ifdef FEATURE_LINKED_SCOPE
                
                Cell cell;
                STACK_ITER(stack, cell, {
                    instant_referred(stack, cell);
                }, false);

                // instantiating domains n or n^x
                STACK_ITER(stack, cell, {
                    if (qctx && qctx->constraint_mode) break;
                    if (!cell.instance->locked && !instantiated(cell.instance)) {
                        igctx ctx[1];
                        ctx->cctx = cctx;
                        ctx->lid = cell.link_id;
                        ctx->force = false;
                        ctx->expset = expression_set;
                        printf("img gen\n");
                        gen_resp gr = image_generate(stack, cell.instance->image, image_generator_rec, ctx);
                        if (gr.r == true) return respond(q_yield, NULL);
                        else return gr.qr;
                    }
                }, false);

                // checking equals n^2
                if (cctx->data != 0x1)
                    STACK_ITER(stack, cell, {
                        if (!check_eq_eval(stack, cell)) {
                            return respond(q_false, NULL);
                        }
                    }, false);

                

                merge_eval(stack, stack->next); // n^3

                #else

                for (int i = 0; i < stack->current.count; i++) {
                    Cell cell = stack->current.cells[i];
                    if (cell.instance == NULL) continue;
                    for (int j = 0; j < stack->current.count; j++) {
                        Cell cell2 = stack->current.cells[j];
                        if (cell.instance == cell2.instance) continue;
                        if (cell2.instance == NULL) continue;
                        if (cell.link_id == cell2.link_id) {
                            if (!instantiated(cell.instance)) {
                                cell.instance->image = image_deepcopy(stack, cell2.instance->image);
                            } else if (!instantiated(cell2.instance)) {
                                cell2.instance->image = image_deepcopy(stack, cell.instance->image);
                            }
                        }
                    }
                }

                // instantiating domains

                for (int i = 0; i < stack->current.count; i++) {
                    if (qctx->constraint_mode) break;
                    if (cctx->data == 0x1) break;
                    Cell cell = stack->current.cells[i];
                    if (cell.instance == NULL) continue;
                    if (cell.instance->locked) continue;
                    else if (!instantiated(cell.instance)) {
                        igctx ctx[1];
                        ctx->cctx = cctx;
                        ctx->lid = cell.link_id;
                        ctx->force = false;
                        ctx->expset = expression_set;
                        gen_resp gr = image_generate(stack, cell.instance->image, image_generator_rec, ctx);
                        if (gr.r == true) return respond(q_yield, NULL);
                        else return gr.qr;
                    }
                }

                // checking equals

                for (int i = 0; i < stack->current.count; i++) {
                    Cell cell = stack->current.cells[i];
                    if (cell.instance == NULL) continue;
                    for (int j = 0; j < stack->current.count; j++) {
                        Cell cell2 = stack->current.cells[j];
                        if (cell2.instance == NULL) continue;
                        if (cell.link_id == cell2.link_id && cell.instance != cell2.instance) {
                            if (! img_equals(cell.instance->image, cell2.instance->image)) {
                                return respond(q_false, NULL);
                            }
                        }
                    }
                }

                // merging stack

                for (int i = 0; i < stack->current.count; i++) {
                    Cell cell = stack->current.cells[i];
                    if (cell.instance == NULL) continue;
                    Cell next_cell = laure_stack_get_cell_by_lid(stack->next, cell.link_id, true);
                    for (int j = 0; j < stack->next->current.count; j++) {
                        Cell next_cell = stack->next->current.cells[j];
                        if (next_cell.instance == NULL) continue;
                        if (next_cell.link_id == cell.link_id) {
                            next_cell.instance->image = image_deepcopy(stack, cell.instance->image);
                        }
                    }
                }

                #endif
                nstack = stack->next;
            } else {
                nstack = stack->next;
            }
            
            if (qctx) {
                qctx->mark = true;
            }

            qcontext *qctx_new = qctx ? qctx->next : qctx;

            control_ctx *cctx_new = control_new(nstack, qctx_new, vpk, cctx->data, cctx->no_ambig);
            qresp qr = laure_eval(cctx_new, qctx_new ? qctx_new->expset : NULL);
            free(cctx_new);

            return qr;
        }
    }

    cctx->silent = false;

    laure_expression_t *ent_exp = expression_set->expression;
    expression_set = expression_set->next;

    if (ent_exp->s) {
        laure_trace_add(ent_exp->s, EXPT_NAMES[ent_exp->t]);
    }

    #ifdef DEBUG
    printf("[%d] %s:%s %s %s (%d)\n", stack->current.id, GRAY_COLOR, YELLOW_COLOR, ent_exp->s, NO_COLOR, ent_exp->t);
    #endif

    switch(ent_exp->t) {
        case let_assert: {

            laure_expression_t *lvar = laure_expression_set_get_by_idx(ent_exp->ba->set, 0);
            laure_expression_t *rvar = laure_expression_set_get_by_idx(ent_exp->ba->set, 1);
            
            if (lvar->t == let_var && rvar->t == let_var) {
                Instance *lvar_ins = laure_stack_get(stack, lvar->s);
                Instance *rvar_ins = laure_stack_get(stack, rvar->s);

                if (lvar_ins && rvar_ins) {
                    bool li = instantiated(lvar_ins);
                    bool ri = instantiated(rvar_ins);

                    if (li && ri) {
                        if (img_equals(lvar_ins->image, rvar_ins->image)) {
                            return RESPOND_OK;
                        }
                        return RESPOND_FALSE;
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
                            RESPOND_ERROR("%s is locked", to->name);
                        }

                        struct ImageHead h = read_head(to->image);
                        struct ImageHead oldh = read_head(from->image);

                        if (h.t != oldh.t) return RESPOND_FALSE;

                        struct IntImage *im1 = (struct IntImage*)from->image;
                        struct IntImage *im2 = (struct IntImage*)to->image;

                        if (img_equals(from->image, to->image))
                            return RESPOND_OK;
                        else
                            return RESPOND_FALSE;
                    } else {
                        // Choicepoint is created
                        // due to ambiguation
                        Instance *var1;
                        Instance *var2;
                        if (lvar_ins->locked && !rvar_ins->locked) {
                            var1 = lvar_ins;
                            var2 = rvar_ins;
                        } else if (!lvar_ins->locked || !rvar_ins->locked) {
                            // When both are not locked the smart
                            // cardinality estimation whould be useful (TODO)
                            var1 = rvar_ins;
                            var2 = lvar_ins;
                        } else {
                            RESPOND_ERROR("both %s and %s are locked variables, so no ambiguation may be created", lvar_ins->name, rvar_ins->name);
                        }
                        struct ImgRecCtx *ctx = malloc(sizeof(struct ImgRecCtx));
                        ctx->cctx = cctx;
                        ctx->var = var2;
                        ctx->expset = expression_set;
                        gen_resp gr = image_generate(stack, var1->image, image_rec1, ctx);
                        free(ctx);
                        if (!gr.r) return gr.qr;
                        return RESPOND_YIELD;
                    }
                } else if (lvar_ins || rvar_ins) {
                    if (rvar_ins) {
                        Instance *temp = lvar_ins;
                        lvar_ins = rvar_ins;
                        rvar_ins = temp;
                    }
                    string nvar_name = strdup(rvar->s);
                    Instance *srcvar = lvar_ins;

                    Instance *nvar = instance_deepcopy(stack, nvar_name, srcvar);
                    nvar->locked = false;

                    Cell cell;
                    cell.instance = nvar;
                    cell.link_id = laure_stack_get_uid(stack);

                    laure_stack_insert(stack, cell);
                    return RESPOND_OK;
                } else {
                    RESPOND_ERROR("both variables %s and %s are undefined and cannot be asserted", lvar->s, rvar->s);
                }
            } else if (lvar->t == let_var || rvar->t == let_var) {
                string vname = lvar->t == let_var ? lvar->s : rvar->s;
                laure_expression_t *rexpression = lvar->t == let_var ? rvar : lvar;
                Instance *to = laure_stack_get(stack, vname);

                if (!to)
                    RESPOND_ERROR("variable %s is undefined", vname);
                
                if (to->locked)
                    RESPOND_ERROR("%s is locked", vname);
                else {
                    // Expression is sent to image translator
                    struct ImageHead head = read_head(to->image);
                    if (! head.translator) {
                        RESPOND_ERROR("can't assign %s with `%s`", vname, rexpression->s);
                    }
                    bool result = head.translator->invoke(rexpression, to->image, stack);
                    if (!result) return RESPOND_FALSE;
                    return RESPOND_OK;
                }
            } else
                RESPOND_ERROR("can't assert %s with %s", lvar->s, rvar->s);

            break;
        }
        case let_image: {
            laure_expression_t *exp1 = laure_expression_set_get_by_idx(ent_exp->ba->set, 0);
            laure_expression_t *exp2 = laure_expression_set_get_by_idx(ent_exp->ba->set, 1);

            if (exp1->t == let_pred || exp1->t == let_constraint) RESPOND_ERROR("imaging left side cannot be %s", EXPT_NAMES[exp1->t]);
            else if (exp2->t == let_pred && exp1->t == let_var) {
                if (! exp2->flag) return respond(q_error, "primitive expected");
                Instance *ins = laure_stack_get(stack, exp1->s);
                if (! ins) {
                    struct PredicateImage *img = (struct PredicateImage*) laure_apply_pred(exp2, stack);
                    img->is_primitive = true;
                    if (! img) return respond(q_error, "hint in predicate primitive is undefined");
                    Instance *prim = instance_new(exp1->s, NULL, img);
                    prim->repr = predicate_repr;
                    Cell cell; cell.instance = prim; cell.link_id = laure_stack_get_uid(stack);
                    laure_stack_insert(stack, cell);
                    return RESPOND_OK;
                } else {
                    printf("todo\n");
                }
            }

            if (exp1->t == let_var && exp2->t == let_var) {

                Instance *var1 = laure_stack_get(stack, exp1->s);
                Instance *var2 = laure_stack_get(stack, exp2->s);

                if (var1 && var2)  {
                    // Check variables derivation
                    if (var1 == var2) return RESPOND_OK;
                    
                    if (instantiated(var1) && instantiated(var2)) {
                        bool eq = img_equals(var1->image, var2->image);
                        return eq ? RESPOND_OK : RESPOND_FALSE;
                    }

                    Instance *derived = var1;
                    while ((derived = derived->derived) != NULL) {
                        if (str_eq(derived->name, var2->name)) return RESPOND_OK;
                    }

                    return RESPOND_FALSE;
                } else if (var1 || var2) {
                    Instance *from;
                    string    new_var_name;
                    uint nesting;

                    if (var1) {
                        from = var1;
                        new_var_name = exp2->s;
                        nesting = exp1->flag;
                    } else {
                        from = var2;
                        new_var_name = exp1->s;
                        nesting = exp2->flag;
                    }

                    Instance *ins = instance_deepcopy(stack, new_var_name, from);

                    if (nesting) {
                        while (nesting) {
                            void *img = array_u_new(ins);
                            ins = instance_new(strdup("el"), NULL, img);
                            ins->repr = array_repr;
                            nesting--;
                        }
                        ins->name = strdup(new_var_name);
                        ins->repr = array_repr;
                    }
                    
                    Cell cell;
                    cell.instance = ins;
                    cell.link_id = laure_stack_get_uid(stack);
                    cell.instance->locked = false;
                    laure_stack_insert(stack, cell);
                    return RESPOND_OK;
                } else
                    RESPOND_ERROR("both variables %s and %s are unknown", exp1->s, exp2->s);
                
            } else {
                RESPOND_ERROR("invalid imaging %s to %s", exp1->s, exp2->s);
            }
            break;
        }
        case let_name: {
            laure_expression_t *v1_exp = laure_expression_set_get_by_idx(ent_exp->ba->set, 0);
            laure_expression_t *v2_exp = laure_expression_set_get_by_idx(ent_exp->ba->set, 1);
            Cell v1 = laure_stack_get_cell_only_locals(stack, v1_exp->s);
            Cell v2 = laure_stack_get_cell_only_locals(stack, v2_exp->s);
            if (!v1.instance && !v2.instance) {
                return respond(q_error, "naming impossible, neither left nor right is known");
            } else if (v1.instance && v2.instance) {
                // Both variables are known
                // assert them
                if (v1.instance == v2.instance) return RESPOND_OK;
                if (instantiated(v1.instance) || instantiated(v2.instance)) {
                    qresp result = img_equals(v1.instance->image, v2.instance->image) ? RESPOND_OK : RESPOND_FALSE;
                    return result;
                } else {
                    laure_stack_change_lid(stack, v2_exp->s, v1.link_id);
                    return RESPOND_OK;
                }
            } else {
                Instance *renamed = v1.instance ? v1.instance : v2.instance;
                renamed->name = strdup(v1.instance ? v2_exp->s : v1_exp->s);
                renamed->doc = v2_exp->docstring;
                return RESPOND_OK;
            }
        }
        case let_var: {
            if (!vpk) return RESPOND_OK;
            string vname = ent_exp->s;
            if (ent_exp->flag > 0) {
                RESPOND_ERROR("cannot evaluate nesting of %s, use infer op.", vname);
            }
            Instance *var = laure_stack_get(stack, vname);
            if (!var)
                RESPOND_ERROR("%s is undefined", vname);
            vpk->single_var_processor(stack, vname, vpk->payload);
            cctx->silent = true;
            return RESPOND_OK;
        }
        case let_unify: {
            Cell unifcell = laure_stack_get_cell(stack, ent_exp->s);

            if (!unifcell.instance)
                RESPOND_ERROR("%s is undefined", ent_exp->s);
            else if (unifcell.instance->locked)
                RESPOND_ERROR("%s is locked", ent_exp->s);
            
            igctx ctx[1];
            ctx->cctx = cctx;
            ctx->lid = unifcell.link_id;
            ctx->force = false;
            ctx->expset = expression_set;

            gen_resp gr = image_generate(stack, unifcell.instance->image, image_generator_rec, ctx);

            if (!gr.r || (gr.qr.state != q_true && cctx->no_ambig)) {
                return gr.qr;
            }

            return respond(q_yield, 0);
        }
        case let_cut: {
            qctx->cut = true;
            return RESPOND_OK;
        }
        case let_imply: {
            laure_expression_t *fact = ent_exp->ba->set->expression;
            laure_expression_set *implies_for = ent_exp->ba->set->next;
            laure_expression_set *fact_expset = fact->ba->set;

            qcontext *n_fact_qctx = qcontext_new(fact_expset);
            qcontext *n_if_qctx = qcontext_new(implies_for);

            n_fact_qctx->next = n_if_qctx;
            n_if_qctx->next = qcontext_new_shifted(qctx, expression_set);

            laure_stack_t *nstack = laure_stack_clone(stack, true);
            if (is_global(stack))
                nstack->global = nstack;
            nstack->repeat = 2;

            control_ctx *ncctx = create_control_ctx(nstack, n_fact_qctx, vpk, cctx->data, false);

            qresp qr = laure_eval(ncctx, fact_expset);

            if (qr.error) {
                return qr;
            }

            qctx->cut = n_if_qctx->cut;

            if (! qr.state) {
                laure_stack_free(nstack);
                if (n_fact_qctx->mark) {
                    return respond(q_false, NULL);
                } else {
                    return RESPOND_OK;
                }
            } else {
                laure_stack_add_to(nstack, stack);
                laure_stack_free(nstack);
                return respond(q_true, NULL);
            }
        }
        case let_quant: {
            string vname = ent_exp->s;
            laure_expression_set *set = ent_exp->ba->set;
            Cell cell = laure_stack_get_cell(stack, vname);

            if (!cell.instance)
                RESPOND_ERROR("%s is undefined", vname);

            if (!ent_exp->ba->body_len)
                return RESPOND_OK;

            qcontext *q_qctx = qcontext_new(set);

            laure_stack_t *q_stack = laure_stack_clone(stack, false);
            q_stack->repeat = 1;
            
            control_ctx *q_cctx = create_control_ctx(q_stack, q_qctx, NULL, NULL, cctx->no_ambig);

            igctx                 ctx[1];
            ctx->cctx           = q_cctx;
            ctx->lid      = cell.link_id;
            ctx->force           = false;
            ctx->expset = expression_set;

            qresp qr;

            switch (ent_exp->flag) {
                case 1: {
                    // `all`
                    qr = image_generate(q_stack, cell.instance->image, quantif_all_gen, ctx).qr;
                    if (qr.state == q_yield) {
                        qr = respond(q_true, 0);
                    }
                    break;
                }
                case 2: {
                    // `exists`
                    qr = image_generate(q_stack, cell.instance->image, quantif_exists_gen, ctx).qr;
                    if (qr.state == q_yield) {
                        qr = respond(q_false, 0);
                    }
                    break;
                }
                default:
                    RESPOND_ERROR("%zi todo", ent_exp->flag);
            }

            if (qr.state != q_true)
                return qr;
            
            return RESPOND_OK;
        }
        case let_choice_2: {
            laure_expression_t *ptr = NULL;

            bool found = false;
            uint i = 0;
            uint count = laure_expression_get_count(ent_exp->ba->set);

            laure_stack_t *initial_stack = laure_stack_clone(stack, true);

            if (is_global(stack)) initial_stack->global = stack;

            laure_expression_set *_set = ent_exp->ba->set;
            while (_set) {
                ptr = _set->expression;

                laure_expression_set *nexpset = ptr->ba->set;
                laure_stack_t *nstack = laure_stack_clone(initial_stack, true);
                qresp choice_qr = process_choice(count, i, nstack, nexpset, cctx, expression_set);

                if (is_global(stack)) {
                    laure_stack_add_to(nstack, stack);
                }
                laure_stack_free(nstack);

                if (choice_qr.state == q_true) {
                    found = true;
                }

                if (choice_qr.error != NULL) {
                    __free_initial_and_quit(choice_qr.state, choice_qr.error);
                } else if (choice_qr.state == q_stop) {
                    if (i != count - 1) __free_initial_and_quit(q_stop, "");
                    else return respond(q_stop, NULL);
                } else if (choice_qr.state == q_continue) {
                    if (i == count - 1) __free_initial_and_quit(q_continue, 0);
                }

                i++;
                _set = _set->next;
            }

            laure_stack_free(initial_stack);
            return respond(found ? q_true : q_false, 0);
        }

        case let_pred_call: {
            string predicate_name = ent_exp->s;
            Instance *predicate_ins = laure_stack_get(stack, predicate_name);

            if (!predicate_ins)
                RESPOND_ERROR("predicate %s is undefined", predicate_name);
            
            enum ImageT call_t = read_head(predicate_ins->image).t;
            if (! (call_t == PREDICATE_FACT || call_t == CONSTRAINT_FACT)) {
                RESPOND_ERROR("%s is neither predicate nor constraint", predicate_ins->name);
            }
            
            bool is_constraint = (call_t == CONSTRAINT_FACT);
            bool turn_off_consmode = is_constraint && !qctx->constraint_mode;

            struct PredicateImage *pred_img = (struct PredicateImage*) predicate_ins->image;

            if (pred_img->is_primitive) {
                predicate_addvar(pred_img, ent_exp);
                return RESPOND_OK;
            }
            
            if (is_constraint) {
                qctx->constraint_mode = true;
            } else if (qctx->constraint_mode) {
                // RESPOND_ERROR("cannot call predicate (%s) inside constraint body", predicate_ins->name);
            }

            if (pred_img->variations->finals == NULL) {
                // first call constructs predicates
                void **pfs = malloc(sizeof(void*) * pred_img->variations->len);
                for (int i = 0; i < pred_img->variations->len; i++) {
                    struct PredicateImageVariation pv = pred_img->variations->set[i];
                    predfinal *pf = get_pred_final(pv);
                    pfs[i] = pf;
                }
                pred_img->variations->finals = (predfinal**)pfs;
            }

            bool need_more = false;
            bool found     = false;

            laure_stack_t *initial_stack = laure_stack_clone(stack, false);

            for (int variation_idx = 0; variation_idx < pred_img->variations->len; variation_idx++) {
                qcontext *nextqctx = qcontext_new(qctx->next);
                nextqctx->constraint_mode = qctx->constraint_mode;
                nextqctx->expset = expression_set;
                nextqctx->next = qctx->next;

                qcontext *nqctx = qcontext_new(NULL);
                nqctx->next = nextqctx;
                nqctx->expset = NULL;
                nqctx->constraint_mode = qctx->constraint_mode;

                laure_stack_t *prev_stack = laure_stack_clone(initial_stack, false);
                laure_stack_t *new_stack = laure_stack_init(prev_stack);
                
                predfinal *pf = pred_img->variations->finals[variation_idx];

                if (pf->t == PF_C) {
                    // C source predicate

                    if (ent_exp->ba->body_len != pf->c.argc && pf->c.argc != -1) {
                        // __query_free_scopes_nqctx;
                        RESPOND_ERROR("predicate %s got %d args, expected %d", predicate_ins->name, ent_exp->ba->body_len, pf->c.argc);
                    }

                    preddata *pd = preddata_new(stack);

                    for (int idx = 0; idx < ent_exp->ba->body_len; idx++) {
                        laure_expression_t *arg_exp = laure_expression_set_get_by_idx(ent_exp->ba->set, idx);

                        switch (arg_exp->t) {
                            case let_var: {
                                string argn = strdup(pf->c.hints[idx]->name);
                                string varname = arg_exp->s;

                                if isanonvar(varname)
                                    varname = laure_stack_get_uname(stack);

                                Cell arg_cell = laure_stack_get_cell_only_locals(prev_stack, varname/*arg_exp->s*/);
                                Instance *arg = arg_cell.instance;
                                
                                if (str_eq(argn, "!")) {
                                    argn = (arg) ? arg->name : "__A";
                                }

                                Cell new_cell;

                                if (arg && arg->locked) {
                                    __query_free_scopes_nqctx;
                                    RESPOND_ERROR("instance %s is locked", arg->name);
                                } else if (! arg) {
                                    if (pf->c.hints[idx]->hint) {
                                        Instance *hint_instance = pf->c.hints[idx]->hint;
                                        arg = instance_deepcopy(stack, argn, hint_instance);
                                    } else {
                                        __query_free_scopes_nqctx;
                                        RESPOND_ERROR("specification of %s's argument %s is needed", predicate_ins->name, argn);
                                    }

                                    Cell arg_cell_in_stack;
                                    arg_cell_in_stack.instance = instance_deepcopy(stack, varname, arg);
                                    arg_cell_in_stack.link_id = laure_stack_get_uid(prev_stack);
                                    arg_cell_in_stack.instance->doc = arg_exp->docstring;

                                    if (is_global(stack)) {
                                        Cell glob = laure_stack_get_cell(stack, varname);
                                        if (glob.instance == NULL)
                                            laure_stack_insert(stack, arg_cell_in_stack);
                                        else
                                            arg_cell_in_stack.link_id = glob.link_id;
                                    }

                                    new_cell.link_id = arg_cell_in_stack.link_id;
                                    new_cell.instance = arg;
                                    laure_stack_insert(prev_stack, arg_cell_in_stack);
                                } else {
                                    arg = instance_deepcopy(stack, argn, arg);
                                    new_cell.instance = arg;
                                    new_cell.link_id = arg_cell.link_id;
                                }

                                laure_stack_insert(new_stack, new_cell);

                                struct predicate_arg pa = {idx, arg, true, arg_cell.link_id};
                                preddata_push(pd, pa);
                                break;
                            }
                            default: {
                                Instance *hint_instance = pf->c.hints[idx]->hint;

                                if (!hint_instance)
                                    RESPOND_ERROR("predicate %s can't resolve meaning of %s", predicate_ins->name, arg_exp->s);
                                
                                struct predicate_arg pa;
                                
                                Instance *arg = instance_deepcopy(stack, pf->c.hints[idx]->name, hint_instance);
                                if (! arg->image)
                                    RESPOND_ERROR("specification of %s is needed in %s call", arg_exp->s, predicate_ins->name);

                                bool result = read_head(arg->image).translator->invoke(arg_exp, arg->image, prev_stack);

                                if  (!result) {
                                    __query_free_scopes_nqctx;
                                    return RESPOND_FALSE;
                                }

                                pa.index = idx;
                                pa.arg = arg;
                                pa.is_instance = true;
                                pa.link_id = 0;

                                Cell cell;
                                cell.instance = arg;
                                cell.link_id = laure_stack_get_uid(prev_stack);

                                laure_stack_insert(new_stack, cell);
                                preddata_push(pd, pa);
                                break;
                            }
                        }
                    }

                    if (pf->c.argc != -1) {
                        for (int j = 0; j < pf->c.argc; j++) {
                            struct PredicateCImageHint *hint = pf->c.hints[j];
                            Instance *in_pd = pd_get_arg(pd, j);
                            if (!in_pd) {
                                printf("panic\n");
                                exit(0);
                            }
                        }
                    }

                    if (ent_exp->ba->has_resp) {
                        laure_expression_t *resp_exp = laure_expression_set_get_by_idx(ent_exp->ba->set, ent_exp->ba->body_len);
                        if (!resp_exp) {
                            Instance *hint_instance = pf->c.resp_hint;
                            if (!hint_instance)
                                RESPOND_ERROR("specification of %s's response is needed (response argument is anonymous now)", predicate_ins->name);
                            pd->resp = instance_deepcopy(stack, strdup("$R"), hint_instance);
                        } else if (str_eq(pf->c.resp_hint, "$")) {
                            pd->resp = resp_exp;
                        } else {
                            switch (resp_exp->t) {
                                case let_var: {
                                    string respn = resp_exp->s;
                                    Cell resp_cell = laure_stack_get_cell_only_locals(prev_stack, respn);
                                    Instance *resp = resp_cell.instance;

                                    Cell new_cell;

                                    if (resp && resp->locked) {
                                        __query_free_scopes_nqctx;
                                        RESPOND_ERROR("instance of %s is locked", respn);
                                    } else if (! resp) {
                                        Instance *hint_instance = pf->c.resp_hint;

                                        if (! hint_instance) {
                                            __query_free_scopes_nqctx;
                                            RESPOND_ERROR("specification of %s's response is needed", predicate_ins->name);
                                        }

                                        resp = instance_deepcopy(stack, strdup("$R"), hint_instance);

                                        Cell resp_cell_in_stack;
                                        resp_cell_in_stack.instance = instance_deepcopy(stack, respn, resp);
                                        resp_cell_in_stack.link_id = laure_stack_get_uid(prev_stack);
                                        resp_cell_in_stack.instance->doc = resp_exp->docstring;

                                        if (is_global(stack)) {
                                            Cell glob = laure_stack_get_cell(stack, respn);
                                            if (!glob.instance) 
                                                laure_stack_insert(stack, resp_cell_in_stack);
                                            else
                                                resp_cell_in_stack.link_id = glob.link_id;
                                        }

                                        new_cell.link_id = resp_cell_in_stack.link_id;
                                        new_cell.instance = resp;

                                        laure_stack_insert(prev_stack, resp_cell_in_stack);
                                    } else {
                                        resp = instance_deepcopy(stack, strdup("$R"), resp);
                                        new_cell.instance = resp;
                                        new_cell.link_id = resp_cell.link_id;
                                    }

                                    laure_stack_insert(new_stack, new_cell);
                                    pd->resp = resp;
                                    pd->resp_link = resp_cell.link_id;
                                    break;
                                }

                                default: {
                                    Instance *hint_instance = pf->c.resp_hint;

                                    if (! hint_instance) {
                                        __query_free_scopes_nqctx;
                                        RESPOND_ERROR("predicate %s can't resolve meaning of response", predicate_ins->name);
                                    }

                                    Instance *resp = instance_deepcopy(stack, strdup("$R"), hint_instance);
                                    
                                    if (! resp->image)
                                        RESPOND_ERROR("specification of response is needed in %s call", predicate_ins->name);
                                    
                                    bool result = read_head(resp->image).translator->invoke(resp_exp, resp->image, prev_stack);

                                    if (!result) {
                                        __query_free_scopes_nqctx;
                                        return RESPOND_FALSE;
                                    }

                                    Cell cell;
                                    cell.instance = resp;
                                    cell.link_id = laure_stack_get_uid(prev_stack);

                                    laure_stack_insert(new_stack, cell);
                                    pd->resp = resp;
                                    break;
                                }
                            }
                        }
                    } else {
                        pd->resp = NULL;
                    }

                    control_ctx *ncctx = create_control_ctx(new_stack, nqctx, vpk, cctx->data, cctx->no_ambig);
                    qresp resp = pf->c.pred(pd, ncctx);

                    if (turn_off_consmode) {
                        qctx->constraint_mode = false;
                        if (qctx->next) {
                            qctx->next->constraint_mode = false;
                        }
                    }

                    if (resp.state == q_stop || resp.state == q_error || __must_stop(resp.state, resp.error, cctx->no_ambig)) {
                        free(ncctx);
                        __query_free_scopes_nqctx;
                        return resp;
                    }

                    // todo `q_postpone`

                    if (resp.state == q_false || resp.state == q_yield) {
                        if (resp.state == q_yield) {
                            if (resp.error) found = true;
                        }
                        free(ncctx);
                        continue;
                    }

                    qresp continued_resp = laure_eval(ncctx, NULL);
                    qctx->mark = nextqctx->mark;

                    if (continued_resp.state != q_true && continued_resp.state != q_yield && cctx->no_ambig) {
                        return respond(q_false, 0);                        
                    }

                    qcontext *first = nextqctx;
                    while (first && first->expset == NULL) first = first->next;
                    if (first) qctx->cut = first->cut;
                    free(ncctx);

                    if (continued_resp.error != NULL) {
                        return continued_resp;
                    }

                    if (continued_resp.state == q_true || continued_resp.state == q_yield) {
                        need_more = false;
                        found = true;
                    }

                    if (continued_resp.state == q_stop) {
                        if (variation_idx == pred_img->variations->len - 1) return respond(q_stop, NULL);
                        else return respond(q_stop, "");
                    }

                    if (continued_resp.state == q_continue) {
                        need_more = true;
                        found = false;
                    }
                } else {

                    if (ent_exp->ba->body_len != pred_img->header.args->len) {
                        RESPOND_ERROR("predicate %s got %d args, expected %d", predicate_ins->name, ent_exp->ba->body_len, pred_img->header.args->len);
                    }

                    laure_expression_set *body = pf->interior.body;
                    body = laure_expression_compose(body);
                    nqctx->expset = body;

                    bool all_instantiated = true;

                    for (int j = 0; j < pf->interior.argc; j++) {
                        string argn = pf->interior.argn[j];

                        laure_expression_t *arg_exp = laure_expression_set_get_by_idx(ent_exp->ba->set, j);

                        switch (arg_exp->t) {
                            case let_var: {
                                string orig_vname = arg_exp->s;
                                if isanonvar(orig_vname) orig_vname = laure_stack_get_uname(stack);

                                Cell arg_cell = laure_stack_get_cell_only_locals(prev_stack, arg_exp->s);
                                Instance *arg = arg_cell.instance;

                                Cell new_cell;
                                if (arg && arg->locked) {
                                    RESPOND_ERROR("instance %s is locked", orig_vname);
                                } else if (! arg) {
                                    Instance *hint_instance = pred_img->header.args->data[j];
                                    arg = instance_deepcopy(stack, argn, hint_instance);

                                    Cell arg_cell_in_stack;
                                    arg_cell_in_stack.instance = instance_deepcopy(stack, orig_vname, arg);
                                    arg_cell_in_stack.link_id = laure_stack_get_uid(prev_stack);
                                    arg_cell_in_stack.instance->doc = arg_exp->docstring;

                                    if (is_global(stack)) {
                                        Cell glob = laure_stack_get_cell(stack, orig_vname);
                                        if (! glob.instance) {
                                            laure_stack_insert(stack, arg_cell_in_stack);
                                        } else {
                                            arg_cell_in_stack.link_id = glob.link_id;
                                        }
                                    }

                                    new_cell.link_id = arg_cell_in_stack.link_id;
                                    new_cell.instance = arg;
                                    laure_stack_insert(prev_stack, arg_cell_in_stack);
                                } else {
                                    arg = instance_deepcopy(stack, argn, arg_cell.instance);
                                    new_cell.instance = arg;
                                    new_cell.link_id = arg_cell.link_id;
                                }
                                if (! instantiated(new_cell.instance)) all_instantiated = false;
                                laure_stack_insert(new_stack, new_cell);
                                break;
                            }
                            default: {
                                Instance *hint_instance = pred_img->header.args->data[j];
                                if (! hint_instance)
                                    RESPOND_ERROR("predicate %s has no hint for argument", predicate_ins->name);
                                                                
                                Instance *arg = instance_deepcopy(stack, argn, hint_instance);
                                bool result = read_head(arg->image).translator->invoke(arg_exp, arg->image, prev_stack);

                                if (! result) {
                                    __query_free_scopes_nqctx;
                                    return RESPOND_FALSE;
                                }

                                Cell cell;
                                cell.instance = arg;
                                cell.link_id = laure_stack_get_uid(prev_stack);

                                laure_stack_insert(new_stack, cell);
                                if (! instantiated(cell.instance)) all_instantiated = false;
                                break;
                            }
                        }
                    } 

                    if (pf->interior.respn) {
                        laure_expression_t *resp_exp = laure_expression_set_get_by_idx(ent_exp->ba->set, ent_exp->ba->body_len);
                        string respn = pf->interior.respn;

                        if (! resp_exp) {
                            Instance *resp = instance_deepcopy(stack, respn, pred_img->header.resp);
                            Cell cell;
                            cell.instance = resp;
                            cell.link_id = laure_stack_get_uid(prev_stack);
                            laure_stack_insert(new_stack, cell);
                            if (! instantiated(cell.instance)) all_instantiated = false;
                        } else {
                            switch (resp_exp->t) {
                                case let_var: {
                                    string orig_respn = resp_exp->s;
                                    Cell resp_cell = laure_stack_get_cell_only_locals(prev_stack, orig_respn);
                                    Instance *resp = resp_cell.instance;

                                    Cell new_cell;
                                    if (resp && resp->locked) {
                                        RESPOND_ERROR("instance of %s is locked", orig_respn);
                                    } else if (! resp) {
                                        Instance *hint_instance = pred_img->header.resp;

                                        if (! hint_instance)
                                            RESPOND_ERROR("predicate %s has no hint for response", predicate_ins->name);
                                        
                                        resp = instance_deepcopy(stack, respn, hint_instance);

                                        Cell resp_cell_in_stack;
                                        resp_cell_in_stack.instance = instance_deepcopy(stack, orig_respn, resp);
                                        resp_cell_in_stack.link_id = laure_stack_get_uid(prev_stack);
                                        resp_cell_in_stack.instance->doc = resp_exp->docstring;

                                        if (is_global(stack)) {
                                            Cell glob = laure_stack_get_cell(stack, orig_respn);
                                            if (! glob.instance) {
                                                laure_stack_insert(stack, resp_cell_in_stack);
                                            } else {
                                                resp_cell_in_stack.link_id = glob.link_id;
                                            }
                                        }

                                        new_cell.link_id = resp_cell_in_stack.link_id;
                                        new_cell.instance = resp;
                                        laure_stack_insert(prev_stack, resp_cell_in_stack);
                                    } else {
                                        resp = instance_deepcopy(stack, respn, resp);
                                        new_cell.instance = resp;
                                        new_cell.link_id = resp_cell.link_id;
                                    }

                                    laure_stack_insert(new_stack, new_cell);
                                    if (! instantiated(new_cell.instance)) all_instantiated = false;
                                    break;
                                }
                                default: {
                                    Instance *hint_instance = pred_img->header.resp;
                                    if (! hint_instance)
                                        RESPOND_ERROR("predicate %s has no hint for response", predicate_ins->name);
                                    
                                    Instance *resp = instance_deepcopy(stack, respn, hint_instance);
                                    bool result = read_head(resp->image).translator->invoke(resp_exp, resp->image, prev_stack);

                                    if (! result) {
                                        __query_free_scopes_nqctx;
                                        return RESPOND_FALSE;
                                    }

                                    Cell cell;
                                    cell.instance = resp;
                                    cell.link_id = laure_stack_get_uid(prev_stack);

                                    laure_stack_insert(new_stack, cell);
                                    if (! instantiated(cell.instance)) all_instantiated = false;
                                    break;
                                }
                            }
                        }
                    }

                    nqctx->expset             = body;
                    new_stack->next           = prev_stack;

                    control_ctx *ncctx = control_new(new_stack, nqctx, vpk, cctx->data, cctx->no_ambig);
                    if (all_instantiated) {
                        ncctx->no_ambig = true;
                    }
                    LAURE_RECURSION_DEPTH++;

                    if (LAURE_RECURSION_DEPTH > LAURE_RECURSION_DEPTH_LIMIT) {
                        // printf("Recursion depth limit of %d exceeded, running %s\n", LAURE_RECURSION_DEPTH_LIMIT, predicate_ins->name);
                        LAURE_RECURSION_DEPTH = 0;
                        RESPOND_ERROR("recursion depth limit of %d exceeded", LAURE_RECURSION_DEPTH_LIMIT);
                    }
                    
                    qresp resp = laure_eval(ncctx, nqctx->expset);
                    laure_stack_free(new_stack);
                    laure_stack_free(prev_stack);

                    LAURE_RECURSION_DEPTH--;
                    qctx->mark = nextqctx->mark;
                    free(ncctx);

                    if (resp.error != NULL) {
                        resp.error = resp.error ? resp.error : strdup("");
                        return resp;
                    }

                    qcontext *anyqctx = nextqctx;
                    qcontext *realqctx = qctx;
                    while (anyqctx && realqctx) {
                        realqctx->cut = anyqctx->cut;
                        anyqctx = anyqctx->next;
                        realqctx = realqctx->next;
                    }

                    if (nqctx->cut) {
                        return respond(q_true, NULL);
                    }

                    if (resp.state == q_true || resp.state == q_yield) {
                        found = true;
                        if (vpk && !vpk->do_process) {
                            laure_showcast(stack, vpk);
                        }
                    }

                    if (resp.state == q_error) {
                        // __query_free_scopes_nqctx;
                        return resp;
                    }

                    if (resp.state == q_stop) {
                        if (variation_idx == pred_img->variations->len - 1) {
                            return respond(q_stop, "");
                        } else {
                            return respond(q_stop, "");
                        }
                    }

                    if (resp.state == q_continue) {
                        need_more = true;
                        found = false;
                    }
                }
            }
            if (! found) {
                return (need_more ? respond(q_continue, NULL) : RESPOND_FALSE);
            } else
                return RESPOND_TRUE;
        }
        case let_pred:
        case let_constraint: {
            apply_result_t result = laure_consult_predicate(LAURE_SESSION, stack, ent_exp, LAURE_CURRENT_ADDRESS);
            if (result.status)
                return RESPOND_OK;
            else
                return respond(q_error, result.error);
            break;
        }
        case let_set: {
            qcontext *nqctx = qcontext_new(ent_exp->ba->set);
            nqctx->next = qcontext_new_shifted(qctx, expression_set);
            laure_stack_t *private_stack = laure_stack_clone(stack, true);
            private_stack->next = stack;
            control_ctx *ncctx = control_new(private_stack, nqctx, vpk, cctx->data, cctx->no_ambig);
            qresp resp = laure_eval(ncctx, ncctx->qctx->expset);
            free(nqctx->next);
            free(nqctx);
            free(ncctx);
            laure_stack_free(private_stack);
            return resp;
        }
        default: break;
    }

    RESPOND_ERROR("can't evaluate", 0);
}