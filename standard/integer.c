#include "standard.h"

DECLARE(laure_predicate_integer_plus) {
    Instance *var1 = pd_get_arg(pd, 0);
    Instance *var2 = pd_get_arg(pd, 1);
    Instance *sum = pd->resp;

    cast_image(var1_im, struct IntImage) var1->image;
    cast_image(var2_im, struct IntImage) var2->image;
    cast_image(sum_im, struct IntImage) sum->image;

    bool var1_i = var1_im->state == I;
    bool var2_i = var2_im->state == I;
    bool sum_i  = sum_im->state  == I;

    if (var1_i && var2_i && sum_i) {
        // 3 = 1 + 2
        bigint real_sum[1];
        bigint_init(real_sum);
        bigint_add(real_sum, var1_im->i_data, var2_im->i_data);
        return from_boolean(bigint_cmp(real_sum, sum_im->i_data) == 0);

    } else if (var1_i && var2_i && !sum_i) {
        // X = 1 + 2
        bigint *bi_sum = laure_alloc(sizeof(bigint));
        bigint_init(bi_sum);
        bigint_add(bi_sum, var1_im->i_data, var2_im->i_data);
        INT_ASSIGN(sum_im, bi_sum);
        return True;

    } else if (var1_i && !var2_i && sum_i) {
        // 3 = 1 + X
        bigint *bi = laure_alloc(sizeof(bigint));
        bigint_init(bi);
        bigint_sub(bi, sum_im->i_data, var1_im->i_data);
        INT_ASSIGN(var2_im, bi);
        return True;
        
    } else if (!var1_i && var2_i && sum_i) {
        // 3 = X + 2
        bigint *bi = laure_alloc(sizeof(bigint));
        bigint_init(bi);
        bigint_sub(bi, sum_im->i_data, var2_im->i_data);
        INT_ASSIGN(var1_im, bi);
        return True;

    } else {
        return respond(q_error, "op. int +|-: too ambiguative; unify");
    }
}

DECLARE(laure_constraint_gt) {
    Instance *i = pd_get_arg(pd, 0);
    Instance *gt = pd_get_arg(pd, 1);

    if (read_head(i->image).t == ATOM) {
        assert(read_head(gt->image).t == ATOM);
        // check size
        cast_image(l,  struct AtomImage) i->image;
        cast_image(r,  struct AtomImage) gt->image;
        
        uint l_sz, r_sz;
        if (l->single) l_sz = 1;
        else l_sz = l->mult->amount;
        if (r->single) r_sz = 1;
        else r_sz = r->mult->amount;

        return from_boolean(l_sz > r_sz);
    }

    cast_image(i_im, struct IntImage) i->image;
    cast_image(gt_im, struct IntImage) gt->image;

    bool i_i = i_im->state == 1;
    bool gt_i = gt_im->state == 1;

    if (i_i && gt_i) {
        return from_boolean(bigint_cmp(i_im->i_data, gt_im->i_data) > 0);
    } else if (!i_i && gt_i) {
        if (i_im->u_data->lborder.t == SECLUDED && bigint_cmp(gt_im->i_data, i_im->u_data->lborder.data) == 0) 
            return True;
        if (i_im->u_data->rborder.t != INFINITE) {
            if (i_im->u_data->rborder.t == INCLUDED) {
                if (bigint_cmp(i_im->u_data->rborder.data, gt_im->i_data) <= 0)
                    return False;
            } else {
                if (bigint_cmp(i_im->u_data->rborder.data, gt_im->i_data) < 0)
                    return False;
            }
        }
        struct IntValue v;
        v.t = SECLUDED;
        v.data = bigint_copy(gt_im->i_data);
        int_domain_gt(i_im->u_data, v);
        return True;
    } else if (i_i && !gt_i) {
        struct IntValue v;
        v.t = SECLUDED;
        v.data = bigint_copy(i_im->i_data);
        int_domain_lt(gt_im->u_data, v);
        return True;
    } else {
        printf("todo gt indefinite\n");
        return True;
    }
}

DECLARE(laure_constraint_gte) {
    Instance *i = pd_get_arg(pd, 0);
    Instance *gt = pd_get_arg(pd, 1);

    if (read_head(i->image).t == ATOM) {
        assert(read_head(gt->image).t == ATOM);
        // check size
        cast_image(l, struct AtomImage) i->image;
        cast_image(r, struct AtomImage) gt->image;
        
        uint l_sz, r_sz;
        if (l->single) l_sz = 1;
        else l_sz = l->mult->amount;
        if (r->single) r_sz = 1;
        else r_sz = r->mult->amount;

        return from_boolean(l_sz >= r_sz);
    }

    cast_image(i_im, struct IntImage) i->image;
    cast_image(gt_im, struct IntImage) gt->image;

    bool i_i = i_im->state == I;
    bool gt_i = gt_im->state == I;

    if (i_i && gt_i) {
        return from_boolean(bigint_cmp(i_im->i_data, gt_im->i_data) >= 0);
    } else if (!i_i && gt_i) {
        if (i_im->u_data->rborder.t != INFINITE) {
            if (bigint_cmp(i_im->u_data->rborder.data, gt_im->i_data) < 0) return False;
        }
        struct IntValue v;
        v.t = INCLUDED;
        v.data = bigint_copy(gt_im->i_data);
        int_domain_gt(i_im->u_data, v);
        return True;
    } else if (i_i && !gt_i) {
        if (!int_check(gt_im, i_im->i_data)) return False;
        struct IntValue v;
        v.t = INCLUDED;
        v.data = bigint_copy(i_im->i_data);
        int_domain_lt(gt_im->u_data, v);
        return True;
    } else {
        printf("todo gte indefinite\n");
        return True;
    }
}

DECLARE(laure_predicate_integer_multiply) {
    Instance *var1 = pd_get_arg(pd, 0);
    Instance *var2 = pd_get_arg(pd, 1);
    Instance *prod = pd->resp;

    Instance *remainder = pd_get_arg(pd, 2);

    cast_image(var1_im, struct IntImage) var1->image;
    cast_image(var2_im, struct IntImage) var2->image;
    cast_image(prod_im, struct IntImage) prod->image;

    bool var1_i = var1_im->state == I;
    bool var2_i = var2_im->state == I;
    bool prod_i  = prod_im->state == I;

    if (var1_i && var2_i && prod_i) {
        bigint real_prod[1];
        bigint_init(real_prod);
        bigint_mul(real_prod, var1_im->i_data, var2_im->i_data);
        bool cmp = bigint_cmp(real_prod, prod_im->i_data) == 0;
        bigint_free(real_prod);
        return from_boolean(cmp);

    } else if (var1_i && var2_i && !prod_i) {
        bigint *bi_prod = laure_alloc(sizeof(bigint));
        bigint_init(bi_prod);
        bigint_mul(bi_prod, var1_im->i_data, var2_im->i_data);
        INT_ASSIGN(prod_im, bi_prod);
        return True;

    } else if (var1_i && !var2_i && prod_i) {
        bigint *bi = laure_alloc(sizeof(bigint));
        bigint_init(bi);
        bigint mod[1];
        if (remainder) {
            bigint_init(mod);
        }
        void *success = bigint_div(bi, prod_im->i_data, var1_im->i_data, remainder == NULL ? NULL : mod);
        if (success == NULL) {
            bigint_free(bi);
            laure_free(bi);
            if (remainder) bigint_free(mod);
            return respond(q_false, NULL);
        }
        if (remainder) {
            int_domain_free(((struct IntImage*)remainder->image)->u_data);
            ((struct IntImage*)remainder->image)->state = I;
            ((struct IntImage*)remainder->image)->i_data = bigint_copy(mod);
            bigint_free(mod);
        }
        
        INT_ASSIGN(var2_im, bi);
        return True;

    } else if (!var1_i && var2_i && prod_i) {
        bigint *bi = laure_alloc(sizeof(bigint));
        bigint_init(bi);
        bigint mod[1];
        if (remainder) {
            bigint_init(mod);
        }
        void *success = bigint_div(bi, prod_im->i_data, var2_im->i_data, remainder == NULL ? NULL : mod);
        if (success == NULL) {
            bigint_free(bi);
            laure_free(bi);
            if (remainder) bigint_free(mod);
            return respond(q_false, NULL);
        }
        if (remainder) {
            int_domain_free(((struct IntImage*)remainder->image)->u_data);
            ((struct IntImage*)remainder->image)->state = I;
            ((struct IntImage*)remainder->image)->i_data = bigint_copy(mod);
            bigint_free(mod);
        }

        INT_ASSIGN(var1_im, bi);
        return True;
    } else {
        return respond(q_error, "op. int *|/: too ambiguative; unify");
    }
}

DECLARE(laure_predicate_sqrt) {
    Instance *n = pd_get_arg(pd, 0);
    Instance *s = pd->resp;

    Instance *rounding_optional = pd_get_arg(pd, 1);
    enum Rounding rounding = RoundingDown;

    if (rounding_optional) {
        cast_image(r_img, struct AtomImage) rounding_optional->image;
        int r = laure_resolve_enum_atom(r_img->atom, ROUNDING_ATOMU, rounding_atomu_size());
        if (r < 0)
            RESPOND_ERROR(signature_err, NULL, "invalid rounding atom");
        rounding = (enum Rounding)r;
    }

    cast_image(n_img, struct IntImage)  n->image;
    cast_image(s_img, struct IntImage)  s->image;

    if (instantiated(n) && instantiated(s)) {
        bigint bi[1];
        bigint_init(bi);
        bigint_sqrt(bi, n_img->i_data, rounding);
        return from_boolean(bigint_cmp(bi, s_img->i_data) == 0);
    } else if (instantiated(n)) {
        bigint *bi = laure_alloc(sizeof(bigint));
        bigint_init(bi);
        bigint_sqrt(bi, n_img->i_data, rounding);
        INT_ASSIGN(s_img, bi);
        return True;
    } else if (instantiated(s)) {
        bigint *bi = laure_alloc(sizeof(bigint));
        bigint_init(bi);
        bigint_mul(bi, s_img->i_data, s_img->i_data);
        INT_ASSIGN(n_img, bi);
        return True;
    } else {
        return respond(q_error, "sqrt: too ambiguative; unify");
    }
}