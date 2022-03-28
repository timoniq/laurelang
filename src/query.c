#include "laurelang.h"
#include "laureimage.h"
#include "predpub.h"

#define YIELD_OK (void*)1
#define YIELD_FAIL (void*)0
#define ANONVAR_NAME "_"

typedef void (*single_proc)(laure_scope_t*, char*, void*);
typedef bool (*sender_rec)(char*, void*);

#define is_global(stack) stack->glob == stack

typedef enum VPKMode {
    INTERACTIVE,
    SENDER,
    SILENT,
} vpk_mode_t;
typedef struct laure_vpk {

    vpk_mode_t mode;
    bool       do_process;
    void      *payload;

    char** tracked_vars;
    int     tracked_vars_len;
    bool    interactive_by_name;

    single_proc single_var_processor;
    sender_rec  sender_receiver;

} var_process_kit;

// Start evaluating search tree
qresp laure_start(control_ctx *cctx, laure_expression_set *expset) {
    laure_expression_t *exp;
    qcontext *qctx = cctx->qctx;
    EXPSET_ITER(expset, exp, {
        qresp response = laure_eval(cctx, exp, _set->next);
        if (
            response.state == q_yield 
            || response.state == q_error
        ) {
            return response;
        } else if (response.state == q_false) {
            return respond(q_yield, YIELD_FAIL);
        }
    });
    return respond(q_yield, YIELD_OK);
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

#define UNPACK_CCTX(cctx) \
        laure_scope_t *scope = cctx->scope; \
        var_process_kit *vpk = cctx->vpk; \
        qcontext *qctx = cctx->qctx;


#define _laure_eval_sub_args control_ctx *cctx, laure_expression_t *e, laure_expression_set *expset
qresp laure_eval_assert(_laure_eval_sub_args);
qresp laure_eval_image(_laure_eval_sub_args);
qresp laure_eval_name(_laure_eval_sub_args);
qresp laure_eval_var(_laure_eval_sub_args);
qresp laure_eval_unify(_laure_eval_sub_args);
qresp laure_eval_pred_call(_laure_eval_sub_args);
qresp laure_eval_callable_decl(_laure_eval_sub_args);

qresp laure_eval(control_ctx *cctx, laure_expression_t *e, laure_expression_set *expset) {
    laure_scope_t *scope = cctx->scope;
    var_process_kit *vpk = cctx->vpk;
    qcontext *qctx = cctx->qctx;

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
        case let_var: {
            return laure_eval_var(cctx, e, expset);
        }
        case let_unify: {
            return laure_eval_unify(cctx, e, expset);
        }
        case let_pred_call: {
            return laure_eval_pred_call(cctx, e, expset);
        }
        case let_pred:
        case let_constraint: {
            return laure_eval_callable_decl(cctx, e, expset);
        }
    }
}

struct img_rec_ctx {
    Instance *var;
    control_ctx *cctx;
    laure_expression_set *expset;
    gen_resp (*qr_process)(qresp, struct img_rec_ctx*);
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
    return ctx;
}

gen_resp qr_process_default(qresp response, struct img_rec_ctx* ctx) {
    bool cont = true;
    if (
        (response.error
        || response.state == q_stop)
        && response.state != q_yield
    ) {
        cont = false;
    }
    gen_resp GR = {cont, response};
    return GR;
}

qresp gen_resp_process(gen_resp gr) {
    if (gr.qr.error)
        return gr.qr;
    return RESPOND_YIELD(YIELD_OK);
}

gen_resp image_rec_default(void *image, struct img_rec_ctx *ctx) {
    void *d = ctx->var->image;
    ctx->var->image = image;
    string repr = ctx->var->repr(ctx->var);
    printf("%s\n", repr);
    free(repr);
    qresp response = laure_start(ctx->cctx, ctx->expset);
    return ctx->qr_process(response, ctx);
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
                    RESPOND_ERROR("%s is locked", to->name);
                }

                struct ImageHead h = read_head(to->image);
                struct ImageHead oldh = read_head(from->image);

                if (h.t != oldh.t) return RESPOND_FALSE;

                return image_equals(from->image, to->image) ? RESPOND_TRUE : RESPOND_FALSE;
            } else {
                // choicepoint is created
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
                    RESPOND_ERROR(
                        "both %s and %s are locked variables, so no ambiguation may be created", 
                        lvar_ins->name, rvar_ins->name
                    );
                }

                // context
                struct img_rec_ctx *ctx = img_rec_ctx_create(var2, cctx, expset, qr_process_default);
                // -------
                gen_resp gr = image_generate(scope, var1->image, image_rec_default, ctx);
                free(ctx);

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
            RESPOND_ERROR("both variables %s and %s are undefined and cannot be asserted", lvar->s, rvar->s);
        }
    } else if (lvar->t == let_var || rvar->t == let_var) {
        string vname = lvar->t == let_var ? lvar->s : rvar->s;
        laure_expression_t *rexpression = lvar->t == let_var ? rvar : lvar;
        Instance *to = laure_scope_find_by_key(scope, vname, true);

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
            bool result = head.translator->invoke(rexpression, to->image, scope);
            if (!result) return RESPOND_FALSE;
            return RESPOND_TRUE;
        }
    } else
        RESPOND_ERROR("can't assert %s with %s", lvar->s, rvar->s);
}

/* =-------=
Imaging.
* Never creates a choicepoint
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

    if (exp1->t == let_var && exp2->t == let_var) {

        Instance *var1 = laure_scope_find_by_key(scope, exp1->s, true);
        Instance *var2 = laure_scope_find_by_key(scope, exp2->s, true);

        if (var1 && var2)  {
            // Check variables derivation
            if (var1 == var2) return RESPOND_TRUE;
            return read_head(var1->image).t == read_head(var2->image).t ? RESPOND_TRUE : RESPOND_FALSE;

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

            Instance *ins = instance_deepcopy(scope, new_var_name, from);

            /* !impl arrays
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
            */

           laure_scope_insert(scope, ins);
            return RESPOND_TRUE;
        } else
            RESPOND_ERROR("both variables %s and %s are unknown", exp1->s, exp2->s);
        
    } else {
        RESPOND_ERROR("invalid imaging %s to %s", exp1->s, exp2->s);
    }
}

/* =-------=
Naming.
Renames instance.
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

gen_resp proc_unify_response(qresp resp, struct img_rec_ctx *ctx) {
    if (resp.state != q_true) {
        if (resp.state == q_yield) {}
        else if (resp.error || resp.state == q_stop || ctx->cctx->no_ambig) {
            gen_resp gr = {0, respond(q_yield, 0)};
            return gr;
        }
    }
    gen_resp gr = {1, resp};
    return gr;
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
        RESPOND_ERROR("%s is undefined", e->s);
    else if (to_unif->locked)
        RESPOND_ERROR("%s is locked", e->s);
    struct img_rec_ctx *ctx = img_rec_ctx_create(to_unif, cctx, expset, proc_unify_response);
    gen_resp gr = image_generate(scope, to_unif->image, image_rec_default, ctx);
    return respond(q_yield, gr.qr.state);
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
        RESPOND_ERROR("cannot evaluate nesting of %s, use infer op.", vname);
    
    Instance *var = laure_scope_find_by_key(scope, vname, true);
    if (!var) RESPOND_ERROR("%s is undefined", vname);

    vpk->single_var_processor(scope, vname, vpk->payload);
    cctx->silent = true;
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

// Predicate call argument procession
#define ARGPROC_RES string
#define ARGPROC_RET_FALSE (void*)1
#define ARGPROC_RES_(R, OK, ERROR, RETFALSE, err) do {string err = NULL; if (R == 0) OK else if (R == ARGPROC_RET_FALSE) {RETFALSE} else {err = (string)R; ERROR;}} while (0)
#define RECORDER_T(name) void (*name)(Instance *ins, ulong link, struct arg_rec_ctx *ctx)
#define return_str_fmt(fmt, ...) do {char sstr[128]; snprintf(sstr, 128, fmt, __VA_ARGS__); string str = strdup( sstr ); return str;} while (0)
#define isanonvar(__name) (str_eq(__name, ANONVAR_NAME))
ARGPROC_RES pred_call_procvar(
    predfinal *pf, 
    laure_scope_t *scope,
    laure_scope_t *previous_scope,
    string argn,
    Instance *hint_opt,
    laure_expression_t *exp, 
    RECORDER_T(recorder),
    struct arg_rec_ctx *ctx
) {
    switch (exp->t) {
        case let_var: {
            string vname = exp->s;
            if (isanonvar(vname))
                vname = laure_scope_generate_unique_name();
            ulong link[1];
            link[0] = 0;
            Instance *arg = laure_scope_find_by_key_l(scope, vname, link, true);
            if (arg && arg->locked) {
                return_str_fmt("%s is locked", arg->name);
            } else if (! arg) {
                if (hint_opt) {
                    arg = instance_deepcopy(scope, argn, hint_opt);
                } else {
                    return_str_fmt("specification of %s is needed", vname);
                }
                ulong preset_link = 0;
                if (is_global(scope)) {
                    ulong l[1];
                    Instance *glob = laure_scope_find_by_key_l(scope, vname, l, true);
                    if (! glob) {
                        linked_scope_t *linked = laure_scope_insert(scope->glob, instance_deepcopy(scope->glob, vname, arg));
                        *l = linked->link;
                    }
                    preset_link = l[0];
                }
                if (preset_link > 0) {
                    laure_scope_insert_l(previous_scope, arg, preset_link);
                    *link = preset_link;
                } else
                    laure_scope_insert(previous_scope, arg);
            } else {
                arg = instance_deepcopy(scope, argn, arg);
            }
            recorder(arg, link[0] ? link[0] : laure_scope_generate_link(), ctx);
            break;
        }
        default: {
            if (! hint_opt)
                return_str_fmt("cannot resolve meaning of %s; add hint", exp->s);
            Instance *arg = instance_deepcopy(scope, argn, hint_opt);
            if (! arg->image)
                return_str_fmt("specification of %s is needed", argn);
            bool result = read_head(arg->image).translator->invoke(exp, arg->image, previous_scope);

            if (! result) {
                printf("instance must be freed\n");
                return ARGPROC_RET_FALSE;
            }
            recorder(arg, laure_scope_generate_link(), ctx);
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
    printf("%lu\n", link);
    laure_scope_insert_l(ctx->new_scope, ins, link);
}

void cpred_resp_recorder(Instance *ins, ulong link, struct arg_rec_ctx *ctx) {
    ctx->pd->resp = ins;
    ctx->pd->resp_link = link;
}

/* =-------=
Predicate/constraint call.
=-------= */
#define pred_call_cleanup do {laure_scope_free(init_scope); laure_scope_free(prev);} while (0)
qresp laure_eval_pred_call(_laure_eval_sub_args) {
    assert(e->t == let_pred_call);
    UNPACK_CCTX(cctx);
    string predicate_name = e->s;
    printf("[ %s ]\n", predicate_name);
    Instance *predicate_ins = laure_scope_find_by_key(scope, predicate_name, true);
    if (! predicate_ins)
        RESPOND_ERROR("predicate %s is undefined", predicate_name);
    enum ImageT call_t = read_head(predicate_ins->image).t;
    if (! (call_t == PREDICATE_FACT || call_t == CONSTRAINT_FACT))
        RESPOND_ERROR("%s is neither predicate nor constraint", predicate_name);
    bool is_constraint = (call_t == CONSTRAINT_FACT);
    bool tfc = is_constraint && !qctx->constraint_mode;

    struct PredicateImage *pred_img = (struct PredicateImage*) predicate_ins->image;
    if (pred_img->is_primitive)
        RESPOND_ERROR("primitive (%s) call is prohibited before instantiation; unify", predicate_name);
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
        pred_img->variations->finals = pfs;
    }

    bool need_more, found = false, false;
    laure_scope_t *init_scope = laure_scope_create_copy(scope);
    for (uint variation_idx = 0; variation_idx < pred_img->variations->len; variation_idx++) {
        predfinal *pf = pred_img->variations->finals[variation_idx];
        laure_scope_t *prev = laure_scope_create_copy(init_scope);

        if (pf->t == PF_C) {
            // C source predicate
            if (e->ba->body_len != pf->c.argc && pf->c.argc != -1) {
                laure_scope_free(init_scope);
                RESPOND_ERROR("predicate %s got %d args, expected %d", predicate_name, e->ba->body_len, pf->c.argc);
            }
            preddata *pd = preddata_new(scope);
            struct arg_rec_ctx actx[1];
            actx->pd = pd;
            actx->prev_scope = prev;

            laure_expression_set *arg_l = e->ba->set;
            for (uint idx = 0; idx < e->ba->body_len; idx++) {
                actx->idx_pd = idx;
                laure_expression_t *argexp = arg_l->expression;

                ARGPROC_RES res = pred_call_procvar(
                    pf, scope, prev, 
                    pf->c.hints[idx]->name, 
                    pf->c.hints[idx]->hint, 
                    argexp, cpred_arg_recorder, 
                    actx);

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
                        RESPOND_ERROR("specification of %s's response is needed", predicate_name);
                    pd->resp = instance_deepcopy(scope, MOCK_NAME, hint);
                    pd->resp_link = 0;
                } else {
                    actx->idx_pd = 0;
                    ARGPROC_RES res = pred_call_procvar(
                        pf, scope, prev,
                        MOCK_NAME,
                        pf->c.resp_hint,
                        exp, cpred_resp_recorder,
                        actx);
                    
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

            qresp resp = pf->c.pred(pd, cctx);
            printf("%d\n", resp.state);

            // Updating args
            for (uint j = 0; j < pd->argc; j++) {
                struct predicate_arg arg = pd->argv[j];
                Instance *arg_ins = laure_scope_find_by_link(prev, arg.link_id, false);
                if (arg_ins)
                    image_equals(arg_ins->image, ((Instance*)arg.arg)->image);
            }
            // Updating resp
            if (pd->resp) {
                printf("%s\n", ((Instance*)pd->resp)->repr(((Instance*)pd->resp)));
                Instance *resp_ins = laure_scope_find_by_link(prev, pd->resp_link, false);
                printf("%d\n", resp_ins != NULL);
                if (resp_ins)
                    image_equals(resp_ins->image, ((Instance*)pd->resp)->image);
            }

            laure_scope_show(prev);
            
            preddata_free(pd);

            if (tfc) {
                qctx->constraint_mode = false;
                if (qctx->next)
                    qctx->next->constraint_mode = false;
            }
            if (resp.state == q_stop || resp.state == q_error) {
                pred_call_cleanup;
                return resp;
            } else if (resp.state == q_yield) {
                laure_scope_free(prev);
                continue;
            }
            cctx->scope = prev;
            laure_scope_show(prev);
            resp = laure_start(cctx, cctx->qctx);
            printf("%d\n", resp.state);
            cctx->scope = scope;
        } else {
            if (e->ba->body_len != pf->interior.argc) {
                laure_scope_free(init_scope);
                RESPOND_ERROR("predicate %s got %d args, expected %d", predicate_name, e->ba->body_len, pf->interior.argc);
            }
            laure_scope_t *nscope = laure_scope_new(scope->glob, prev);
            struct arg_rec_ctx actx[1];
            actx->new_scope = nscope;

            laure_expression_set *arg_l = e->ba->set;
            for (uint idx = 0; idx < e->ba->body_len; idx++) {
                actx->idx_pd = idx;
                laure_expression_t *argexp = arg_l->expression;

                ARGPROC_RES res = pred_call_procvar(
                    pf, scope, prev, 
                    pf->interior.argn[idx], 
                    pred_img->header.args->data[idx], 
                    argexp, dompred_arg_recorder, 
                    actx);

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
                    Instance *hint = pred_img->header.resp;
                    if (! hint)
                        RESPOND_ERROR("specification of %s's response is needed", predicate_name);
                    
                } else {
                    actx->idx_pd = 0;
                    laure_expression_t *exp = arg_l->expression;

                    ARGPROC_RES res = pred_call_procvar(
                        pf, scope, prev,
                        pf->interior.respn,
                        pred_img->header.resp,
                        exp, dompred_arg_recorder,
                        actx);
                    
                    ARGPROC_RES_(
                        res,
                        {},
                        {pred_call_cleanup; return respond(q_error, err);},
                        {pred_call_cleanup; return RESPOND_FALSE;},
                        err);
                }
            }
            
            laure_expression_set *body = pf->interior.body;
            body = laure_expression_compose(body);

            qcontext *nqctx = qcontext_new(body);
            nqctx->next = qctx;
            nqctx->constraint_mode = is_constraint;

            cctx->scope = nscope;
            cctx->qctx = nqctx;
            laure_scope_show(nscope);
            qresp resp = laure_start(cctx, body);
            laure_scope_show(nscope);
            cctx->qctx = qctx;
        }
    }
}

/* =-------=
Declaration of a callable instance
=-------= */
qresp laure_eval_callable_decl(_laure_eval_sub_args) {
    assert(e->t == let_pred || e->t == let_constraint);
    UNPACK_CCTX(cctx);
    laure_session_t session[1];
    session->scope = scope;
    session->_doc_buffer = NULL;
    memset(session->_included_filepaths, 0, sizeof(void*));
    apply_result_t result = laure_consult_predicate(session, scope, e, LAURE_CURRENT_ADDRESS);
    if (result.status)
        return RESPOND_TRUE;
    else
        return respond(q_error, result.error);
}

control_ctx *control_new(laure_scope_t* scope, qcontext* qctx, var_process_kit* vpk, void* data, bool no_ambig) {
    control_ctx *cctx = malloc(sizeof(control_ctx));
    cctx->scope = scope;
    cctx->qctx = qctx;
    cctx->vpk = vpk;
    cctx->data = data;
    cctx->silent = false;
    cctx->no_ambig = no_ambig;
    return cctx;
}

qcontext *qcontext_new(laure_expression_set *expset) {
    qcontext *qctx = malloc(sizeof(qcontext));
    qctx->constraint_mode = false;
    qctx->expset = expset;
    qctx->next = NULL;
    qctx->cut = false;
    return qctx;
}

void *pd_get_arg(preddata *pd, int index) {
    for (int i = 0; i < pd->argc; i++)
        if (pd->argv[i].index == index)
            return pd->argv[i].arg;
    return NULL;
}