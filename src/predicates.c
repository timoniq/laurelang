#include "predicates.h"
#include "readline/readline.h"

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
        if (i_im->u_data->lborder.t == SECLUDED && bigint_cmp(gt_im->i_data, i_im->u_data->lborder.data) == 0) return respond(q_true, 0);
        if (i_im->u_data->rborder.t != INFINITE) {
            if (i_im->u_data->rborder.t == INCLUDED) {
                if (bigint_cmp(i_im->u_data->rborder.data, gt_im->i_data) <= 0) return respond(q_false, 0);
            } else {
                if (bigint_cmp(i_im->u_data->rborder.data, gt_im->i_data) < 0) return respond(q_false, 0);
            }
        }
        struct IntValue v;
        v.t = SECLUDED;
        v.data = bigint_copy(gt_im->i_data);
        int_domain_gt(i_im->u_data, v);
        return respond(q_true, 0);
    } else if (i_i && !gt_i) {
        struct IntValue v;
        v.t = SECLUDED;
        v.data = bigint_copy(i_im->i_data);
        int_domain_lt(gt_im->u_data, v);
        return respond(q_true, 0);
    } else {
        printf("todo gt indefinite\n");
        return respond(q_false, 0);
    }
}

qresp laure_constraint_gte(preddata *pd, control_ctx* cctx) {
    Instance *i = pd_get_arg(pd, 0);
    Instance *gt = pd_get_arg(pd, 1);

    struct IntImage *i_im = (struct IntImage*)i->image;
    struct IntImage *gt_im = (struct IntImage*)gt->image;

    int i_i = i_im->state == I;
    int gt_i = gt_im->state == I;

    if (i_i && gt_i) {
        return bigint_cmp(i_im->i_data, gt_im->i_data) >= 0 ? respond(q_true, 0) : respond(q_false, 0);
    } else if (!i_i && gt_i) {
        if (i_im->u_data->rborder.t != INFINITE) {
            if (bigint_cmp(i_im->u_data->rborder.data, gt_im->i_data) < 0) return respond(q_false, 0);
        }
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
        printf("todo gte indefinite\n");
        return respond(q_true, 0);
    }
}

qresp laure_predicate_integer_multiply(preddata *pd, control_ctx* cctx) {
    Instance *var1 = pd_get_arg(pd, 0);
    Instance *var2 = pd_get_arg(pd, 1);
    Instance *prod = pd->resp;

    struct IntImage *var1_im = (struct IntImage*)var1->image;
    struct IntImage *var2_im = (struct IntImage*)var2->image;
    struct IntImage *prod_im = (struct IntImage*)prod->image;

    int var1_i = var1_im->state == I;
    int var2_i = var2_im->state == I;
    int prod_i  = prod_im->state == I;

    if (var1_i && var2_i && prod_i) {
        bigint real_prod[1];
        bigint_init(real_prod);
        bigint_mul(real_prod, var1_im->i_data, var2_im->i_data);
        bool cmp = bigint_cmp(real_prod, prod_im->i_data) == 0;
        bigint_free(real_prod);
        return respond((qresp_state)cmp, NULL);

    } else if (var1_i && var2_i && !prod_i) {
        bigint *bi_prod = malloc(sizeof(bigint));
        bigint_init(bi_prod);
        bigint_mul(bi_prod, var1_im->i_data, var2_im->i_data);
        INT_ASSIGN(prod_im, bi_prod);
        return respond(q_true, NULL);

    } else if (var1_i && !var2_i && prod_i) {
        bigint *bi = malloc(sizeof(bigint));
        bigint_init(bi);
        void *success = bigint_div(bi, prod_im->i_data, var1_im->i_data, true);
        if (success == NULL) return respond(q_false, NULL);
        INT_ASSIGN(var2_im, bi);
        return respond(q_true, NULL);

    } else if (!var1_i && var2_i && prod_i) {
        bigint *bi = malloc(sizeof(bigint));
        bigint_init(bi);
        void *success = bigint_div(bi, prod_im->i_data, var2_im->i_data, true);
        if (success == NULL) return respond(q_false, NULL);
        INT_ASSIGN(var1_im, bi);
        return respond(q_true, NULL);
    } else {
        return respond(q_error, "op. int *|/: too ambiguative; unify");
    }
}

qresp laure_predicate_sqrt(preddata *pd, control_ctx *cctx) {
    Instance *n = pd_get_arg(pd, 0);
    Instance *s = pd->resp;
    struct IntImage *n_img = (struct IntImage*) n->image;
    struct IntImage *s_img = (struct IntImage*) s->image;
    if (instantiated(n) && instantiated(s)) {
        bigint bi[1];
        bigint_init(bi);
        bigint_sqrt(bi, n_img->i_data);
        return respond((bigint_cmp(bi, s_img->i_data) == 0) ? q_true : q_false, 0);
    } else if (instantiated(n)) {
        bigint *bi = malloc(sizeof(bigint));
        bigint_init(bi);
        bigint_sqrt(bi, n_img->i_data);
        INT_ASSIGN(s_img, bi);
        return respond(q_true, 0);
    } else if (instantiated(s)) {
        bigint *bi = malloc(sizeof(bigint));
        bigint_init(bi);
        bigint_mul(bi, s_img->i_data, s_img->i_data);
        INT_ASSIGN(n_img, bi);
        return respond(q_true, 0);
    } else {
        return respond(q_error, "sqrt: too ambiguative; unify");
    }
}

qresp laure_predicate_message(preddata *pd, control_ctx *cctx) {
    Instance *ins = pd_get_arg(pd, 0);
    struct ArrayImage *img = (struct ArrayImage*) ins->image;
    if (img->t != ARRAY)
        RESPOND_ERROR(type_err, NULL, "message's argument must be array, not %s", IMG_NAMES[img->t]);
    
    if (img->state) {
        array_linked_t *linked = img->i_data.linked;
        for (uint idx = 0; idx < img->i_data.length && linked; idx++) {
            struct CharImage *ch = (struct CharImage*) linked->data->image;
            int c = ch->c;
            if (c == '\\') {
                linked = linked->next;
                idx++;
                c = laure_convert_ch_esc(((struct CharImage*) linked->data->image)->c);
            }
            char buff[8];
            laure_string_put_char(buff, c);
            printf("%s", buff);
            linked = linked->next;
        }
        printf("\n");
        return respond(q_true, 0);
    } else {
        string s = readline("> ");
        char buff[128];
        snprintf(buff, 128, "\"%s\"", s);
        free(s);
        laure_expression_t exp[1];
        exp->t = let_custom;
        exp->s = buff;
        bool result = img->translator->invoke(exp, img, cctx->scope);
        return respond(result ? q_true : q_false, 0);
    }
}