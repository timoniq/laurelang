#include "predicates.h"
#include <readline/readline.h>

#define __finalize do {/*laure_gc_treep_destroy(local_gc);*/} while (0)

struct SumCtx {
    bigint *sum;
    control_ctx *ctx;
    Instance *var1;
    Instance *var2;
    struct IntImage *check;
    bool found_any;
};

struct Sum2Ctx {
    bigint *known;
    control_ctx *ctx;
    Instance *sum;
    Instance *var;
    struct IntImage *check;
    bool found_any;
};

void sum_ctx_init(struct SumCtx *dst, bigint *sum, control_ctx *ctx, Instance *var1, Instance *var2, struct IntImage *check) {
    dst->sum = sum;
    dst->ctx = ctx;
    dst->var1 = var1;
    dst->var2 = var2;
    dst->check = check;
    dst->found_any = false;
}

void sum_ctx2_init(struct Sum2Ctx *dst, bigint *known, control_ctx *ctx, Instance *sum, Instance *var, struct IntImage *check) {
    dst->known = known;
    dst->ctx = ctx;
    dst->sum = sum;
    dst->var = var;
    dst->check = check;
    dst->found_any = false;
}

gen_resp integer_plus_sum_known_rec(struct IntImage *var1_im, struct SumCtx *context) {
    bigint * sum = context->sum;
    bigint *var2 = malloc(sizeof(bigint));

    bigint_init(var2);
    bigint_sub(var2, sum, var1_im->i_data);

    if (!int_check(context->check, var2)) {
        free(var2->words);
        free(var2);
        gen_resp gr = {1, respond(q_false, NULL)};
        return gr;
    };

    laure_stack_t* stack = context->ctx->stack;

    laure_stack_t* orig_heap = (laure_stack_t*)context->ctx->data;

    ((struct IntImage*)context->var1->image)->i_data = var1_im->i_data;
    ((struct IntImage*)context->var1->image)->state = I;

    ((struct IntImage*)context->var2->image)->i_data = var2;
    ((struct IntImage*)context->var2->image)->state = I;

    laure_stack_t *nstack = laure_stack_clone(stack, true);
    control_ctx *cctx = control_new(nstack, context->ctx->qctx, context->ctx->vpk, context->ctx->data);

    qresp r = laure_eval(cctx, context->ctx->qctx->expset);

    free(cctx);

    if (r.state == q_error || r.state == q_stop) {
        gen_resp gr = {0, r};
        return gr;
    }

    gen_resp gr = {1};
    if (r.state == q_true)
        context->found_any = true;
    
    return gr;
};

gen_resp integer_plus_one_known_rec(struct IntImage *sum_im, struct Sum2Ctx *context) {
    bigint *var = malloc(sizeof(bigint));
    bigint_init(var);

    bigint_sub(var, sum_im->i_data, context->known);
    
    if (!int_check(context->check, var)) {
        free(var->words);
        free(var);
        gen_resp gr = {1, respond(q_false, NULL)};
        return gr;
    }

    laure_stack_t* stack = context->ctx->stack;

    ((struct IntImage*)context->sum->image)->i_data = sum_im->i_data;
    ((struct IntImage*)context->sum->image)->state = I;

    ((struct IntImage*)context->var->image)->i_data = var;
    ((struct IntImage*)context->var->image)->state = I;

    laure_stack_t *nstack = laure_stack_clone(stack, true);

    control_ctx *cctx = control_new(nstack, context->ctx->qctx, context->ctx->vpk, context->ctx->data);

    qresp r = laure_eval(cctx, context->ctx->qctx->expset);

    free(cctx);

    if (r.error != NULL || r.state == q_stop) {
        gen_resp gr = {0, r};
        return gr;
    }

    gen_resp gr = {1};
    if (r.state == q_true || r.state == q_true_s || r.state == q_continue) {
        context->found_any = true;
    }

    return gr;
};

gen_resp integer_mul_sum_known_rec(struct IntImage *var1_im, struct SumCtx *context) {
    bigint * prod = context->sum;
    bigint *var2 = malloc(sizeof(bigint));
    bigint_init(var2);

    void *success = bigint_div(var2, prod, var1_im->i_data, true);

    if (!success) {
        free(var2->words);
        free(var2);
        // bigint_free(context->ctx->heap, var2);
        gen_resp gr = {1, respond(q_false, NULL)};
        return gr;
    }

    if (!int_check(context->check, var2)) {
        free(var2->words);
        free(var2);
        gen_resp gr = {1, respond(q_false, NULL)};
        return gr;
    };

    laure_stack_t* stack = context->ctx->stack;

    laure_stack_t* orig_heap = (laure_stack_t*)context->ctx->data;

    ((struct IntImage*)context->var1->image)->i_data = var1_im->i_data;
    ((struct IntImage*)context->var1->image)->state = I;

    ((struct IntImage*)context->var2->image)->i_data = var2;
    ((struct IntImage*)context->var2->image)->state = I;

    // 

    laure_stack_t *nstack = laure_stack_clone(stack, true);
    control_ctx *cctx = control_new(nstack, context->ctx->qctx, context->ctx->vpk, context->ctx->data);

    qresp r = laure_eval(cctx, context->ctx->qctx->expset);

    free(cctx);

    if (r.state == q_error || r.state == q_stop) {
        gen_resp gr = {0, r};
        return gr;
    }

    gen_resp gr = {1};
    if (r.state == q_true)
        context->found_any = true;
    
    return gr;
};

gen_resp integer_mul_one_known_rec(struct IntImage *prod_im, struct Sum2Ctx *context) {
    bigint *var = malloc(sizeof(bigint));
    bigint_init(var);

    void *success = bigint_div(var, prod_im->i_data, context->known, true);
    if (!success) {
        free(var->words);
        free(var);
        gen_resp gr = {1, respond(q_false, NULL)};
        return gr;
    }
    
    if (!int_check(context->check, var)) {
        free(var->words);
        free(var);
        gen_resp gr = {1, respond(q_false, NULL)};
        return gr;
    }

    laure_stack_t* stack = context->ctx->stack;

    ((struct IntImage*)context->sum->image)->i_data = prod_im->i_data;
    ((struct IntImage*)context->sum->image)->state = I;

    ((struct IntImage*)context->var->image)->i_data = var;
    ((struct IntImage*)context->var->image)->state = I;

    laure_stack_t *nstack = laure_stack_clone(stack, true);
    control_ctx *cctx = control_new(nstack, context->ctx->qctx, context->ctx->vpk, context->ctx->data);

    qresp r = laure_eval(cctx, context->ctx->qctx->expset);

    free(cctx);

    if (r.error != NULL || r.state == q_stop) {
        gen_resp gr = {0, r};
        return gr;
    }

    gen_resp gr = {1};
    if (r.state == q_true || r.state == q_true_s || r.state == q_continue) {
        context->found_any = true;
    }

    return gr;
};

qresp laure_predicate_integer_plus(preddata *pd, control_ctx* cctx) {
    Instance *var1 = pd_get_arg(pd, 0);
    Instance *var2 = pd_get_arg(pd, 1);
    Instance *sum = pd->resp;

    laure_gc_treep_t *local_gc = NULL;

    ASSERT_IS_INT(local_gc, cctx->stack, var1);
    ASSERT_IS_INT(local_gc, cctx->stack, var2);
    ASSERT_IS_INT(local_gc, cctx->stack, sum);

    struct IntImage *var1_im = (struct IntImage*)var1->image;
    struct IntImage *var2_im = (struct IntImage*)var2->image;
    struct IntImage *sum_im = (struct IntImage*)sum->image;

    int var1_i = var1_im->state == 1;
    int var2_i = var2_im->state == 1;
    int sum_i  = sum_im->state == 1;

    if (var1_i && var2_i && sum_i) {
        // 3 = 1 + 2
        bigint real_sum[1];
        bigint_init(real_sum);
        bigint_add(real_sum, var1_im->i_data, var2_im->i_data);
        __finalize;
        return respond((qresp_state)(bigint_cmp(real_sum, sum_im->i_data) == 0), NULL);

    } else if (var1_i && var2_i && !sum_i) {
        // X = 1 + 2
        bigint *bi_sum = malloc(sizeof(bigint));
        bigint_init(bi_sum);
        bigint_add(bi_sum, var1_im->i_data, var2_im->i_data);
        INT_ASSIGN(sum_im, bi_sum);
        __finalize;
        return respond(q_true, NULL);

    } else if (var1_i && !var2_i && sum_i) {
        // 3 = 1 + X
        bigint *bi = malloc(sizeof(bigint));
        bigint_init(bi);

        bigint_sub(bi, sum_im->i_data, var1_im->i_data);

        INT_ASSIGN(var2_im, bi);
        __finalize;
        return respond(q_true, NULL);
        
    } else if (!var1_i && var2_i && sum_i) {
        // 3 = X + 2
        bigint *bi = malloc(sizeof(bigint));
        bigint_init(bi);

        bigint_sub(bi, sum_im->i_data, var2_im->i_data);

        INT_ASSIGN(var1_im, bi);
        __finalize;
        return respond(q_true, NULL);

    } else if (!var1_i && !var2_i && sum_i) {
        // 3 = X + Y

        struct SumCtx *sum_ctx = malloc(sizeof(struct SumCtx));
        void *nimage = image_deepcopy(cctx->stack, var2->image);
        sum_ctx_init(sum_ctx, sum_im->i_data, cctx, var1, var2, nimage);

        gen_resp gr = image_generate(cctx->stack, var1_im, integer_plus_sum_known_rec, sum_ctx);
        image_free(nimage, true);
        free(sum_ctx);
        __finalize;
        return sum_ctx->found_any ? respond(q_yield, NULL) : respond(q_false, NULL);

    } else if (var1_i && !var2_i && !sum_i) {
        struct Sum2Ctx *sum_ctx = malloc(sizeof(struct Sum2Ctx));
        void *nimage =  image_deepcopy(cctx->stack, var2_im);
        sum_ctx2_init(sum_ctx, var1_im->i_data, cctx, sum, var2, nimage);

        gen_resp gr = image_generate(cctx->stack, sum_im, integer_plus_one_known_rec, sum_ctx);
        image_free(nimage, true);
        free(sum_ctx);
        __finalize;
        return sum_ctx->found_any ? respond(q_yield, NULL) : respond(q_false, NULL);

    } else if (!var1_i && var2_i && !sum_i) {
        struct Sum2Ctx *sum_ctx = malloc(sizeof(struct Sum2Ctx));
        void *nimage = image_deepcopy(cctx->stack, var1_im);
        sum_ctx2_init(sum_ctx, var2_im->i_data, cctx, sum, var1, nimage);

        gen_resp gr = image_generate(cctx->stack, sum_im, integer_plus_one_known_rec, sum_ctx);
        image_free(nimage, true);
        free(sum_ctx);
        __finalize;
        return sum_ctx->found_any ? respond(q_yield, NULL) : respond(q_false, NULL);

    } else {
        __finalize;
        return respond(q_error, "cannot instantiate");
    }
}


qresp laure_constraint_gte(preddata *pd, control_ctx* cctx) {
    Instance *i = pd_get_arg(pd, 0);
    Instance *gt = pd_get_arg(pd, 1);

    struct IntImage *i_im = (struct IntImage*)i->image;
    struct IntImage *gt_im = (struct IntImage*)gt->image;

    int i_i = i_im->state == 1;
    int gt_i = gt_im->state == 1;

    if (i_i && gt_i) {
        return bigint_cmp(i_im->i_data, gt_im->i_data) >= 0 ? respond(q_true, 0) : respond(q_false, 0);
    } else if (!i_i && gt_i) {
        if (!int_check(i_im, gt_im->i_data)) return respond(q_false, 0);
        struct IntValue v;
        v.t = INCLUDED;
        v.data = bigint_copy(gt_im->i_data);
        int_domain_gt(i_im->u_data, v);
        return respond(q_true, 0);
    } else if (i_i && !gt_i) {
        if (!int_check(gt_im, i_im->i_data)) return respond(q_false, 0);
        struct IntValue v;
        v.t = INCLUDED;
        v.data = bigint_copy(i_im->i_data);
        int_domain_lt(gt_im->u_data, v);
        return respond(q_true, 0);
    } else {
        printf("todo\n");
        return respond(q_true, 0);
    }
}

qresp laure_constraint_gt(preddata *pd, control_ctx* cctx) {
    Instance *i = pd_get_arg(pd, 0);
    Instance *gt = pd_get_arg(pd, 1);

    struct IntImage *i_im = (struct IntImage*)i->image;
    struct IntImage *gt_im = (struct IntImage*)gt->image;

    int i_i = i_im->state == 1;
    int gt_i = gt_im->state == 1;

    if (i_i && gt_i) {
        return bigint_cmp(i_im->i_data, gt_im->i_data) > 0 ? respond(q_true, 0) : respond(q_false, 0);
    } else if (!i_i && gt_i) {
        if (!int_check(i_im, gt_im->i_data)) return respond(q_false, 0);
        struct IntValue v;
        v.t = SECLUDED;
        v.data = bigint_copy(gt_im->i_data);
        int_domain_gt(i_im->u_data, v);
        return respond(q_true, 0);
    } else if (i_i && !gt_i) {
        if (!int_check(gt_im, i_im->i_data)) return respond(q_false, 0);
        struct IntValue v;
        v.t = SECLUDED;
        v.data = bigint_copy(i_im->i_data);
        int_domain_lt(gt_im->u_data, v);
        return respond(q_true, 0);
    } else {
        printf("todo\n");
        return respond(q_true, 0);
    }
}
