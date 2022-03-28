#include "predicates.h"

struct sum_ctx_t {
    bigint *sum;
    control_ctx *ctx;
    Instance *var1;
    Instance *var2;
    struct IntImage *check;
    bool found_any;
};

struct sum2_ctx_t {
    bigint *known;
    control_ctx *ctx;
    Instance *sum;
    Instance *var;
    struct IntImage *check;
    bool found_any;
};

struct sum_ctx_t *sum_ctx_new(bigint *sum, control_ctx *ctx, Instance *var1, Instance *var2, struct IntImage *check) {
    struct sum_ctx_t *dst = malloc(sizeof(struct sum_ctx_t));
    dst->sum = sum;
    dst->ctx = ctx;
    dst->var1 = var1;
    dst->var2 = var2;
    dst->check = check;
    dst->found_any = false;
    return dst;
}

struct sum2_ctx_t *sum_ctx2_new(bigint *known, control_ctx *ctx, Instance *sum, Instance *var, struct IntImage *check) {
    struct sum2_ctx_t *dst = malloc(sizeof(struct sum2_ctx_t));
    dst->known = known;
    dst->ctx = ctx;
    dst->sum = sum;
    dst->var = var;
    dst->check = check;
    dst->found_any = false;
    return dst;
}

qresp laure_predicate_integer_plus(preddata *pd, control_ctx* cctx) {
    Instance *var1 = pd_get_arg(pd, 0);
    Instance *var2 = pd_get_arg(pd, 1);
    Instance *sum = pd->resp;

    struct IntImage *var1_im = (struct IntImage*)var1->image;
    struct IntImage *var2_im = (struct IntImage*)var2->image;
    struct IntImage *sum_im =  (struct IntImage*)sum->image;

    int var1_i = var1_im->state == I;
    int var2_i = var2_im->state == I;
    int sum_i  = sum_im->state  == I;

    if (var1_i && var2_i && sum_i) {
        // 3 = 1 + 2
        bigint real_sum[1];
        bigint_init(real_sum);
        bigint_add(real_sum, var1_im->i_data, var2_im->i_data);
        return respond((qresp_state)(bigint_cmp(real_sum, sum_im->i_data) == 0), NULL);

    } else if (var1_i && var2_i && !sum_i) {
        // X = 1 + 2
        bigint *bi_sum = malloc(sizeof(bigint));
        bigint_init(bi_sum);
        bigint_add(bi_sum, var1_im->i_data, var2_im->i_data);
        INT_ASSIGN(sum_im, bi_sum);
        return respond(q_true, NULL);

    } else if (var1_i && !var2_i && sum_i) {
        // 3 = 1 + X
        bigint *bi = malloc(sizeof(bigint));
        bigint_init(bi);

        bigint_sub(bi, sum_im->i_data, var1_im->i_data);

        INT_ASSIGN(var2_im, bi);
        return respond(q_true, NULL);
        
    } else if (!var1_i && var2_i && sum_i) {
        // 3 = X + 2
        bigint *bi = malloc(sizeof(bigint));
        bigint_init(bi);

        bigint_sub(bi, sum_im->i_data, var2_im->i_data);

        INT_ASSIGN(var1_im, bi);
        return respond(q_true, NULL);

    } else {
        return respond(q_error, "op. int +|-: too ambiguative; unify");
    }
}