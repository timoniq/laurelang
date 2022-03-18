#include "laureimage.h"

int char_isnumber(int _c) {
    return _c >= 48 && _c <= 57;
}

ImageHead read_head(void *img) {
    struct ImageHead head;
    memcpy(&head, img, sizeof(struct ImageHead));
    return head;
}

void *get_image(Instance *ins) {
    return ins->image;
}

EnhancedImageHead read_enhanced_head(void *img) {
    struct EnhancedImageHead head;
    memcpy(&head, img, sizeof(struct EnhancedImageHead));
    return head;
}

void *integer_i_new(int data) {
    struct IntImage *img = malloc(sizeof(struct IntImage));
    img->t = INTEGER;
    img->i_data = bigint_create(data);
    img->datatype = BINT;
    img->translator = new_translator(int_translator);
    img->state = I;
    return img;
}

void *integer_u_new() {
    struct IntImage *img = malloc(sizeof(struct IntImage));
    img->t = INTEGER;
    img->u_data = int_domain_new();
    img->datatype = BINT;
    img->state = U;
    img->translator = new_translator(int_translator);
    return img;
}

void *array_i_new(Instance **array, int length) {
    struct ArrayImage *img = malloc(sizeof(struct ArrayImage));

    struct ArrayIData i_data;
    i_data.array = array;
    i_data.length = length;

    img->t = ARRAY;
    img->state = I;
    img->i_data = i_data;
    img->translator = new_translator(array_translator);
    img->arr_el = NULL;
    img->length_lid = 0;
    img->ref_count = 0;
    img->ref = NULL;

    return (void*)img;
}

void *array_u_new(Instance *element) {
    struct ArrayImage *img = malloc(sizeof(struct ArrayImage));

    struct ArrayUData u_data;
    u_data.length = int_domain_new();

    IntValue zero = {INCLUDED, bigint_create(0)};
    int_domain_gt(u_data.length, zero);

    img->t = ARRAY;
    img->arr_el = element;
    img->translator = new_translator(array_translator);
    img->state = U;
    img->u_data = u_data;
    img->length_lid = 0;
    img->ref_count = 0;
    img->ref = NULL;

    return (void*)img;
}

void *abstract_new(string atom) {
    return NULL;
}

struct PredicateImage *predicate_header_new(struct InstanceSet *args, Instance *resp, bool is_constraint) {
    struct PredicateImage *img = malloc(sizeof(struct PredicateImage));

    img->t = !is_constraint ? PREDICATE_FACT : CONSTRAINT_FACT;
    
    struct PredicateHeaderImage header;
    header.args = args;
    header.resp = resp;
    header.nestings = 0;
    header.response_nesting = 0;
    
    img->translator = NULL;
    img->header = header;
    img->variations = pvs_new();
    img->is_primitive = false;
    
    return img;
}

struct ArrayIData array_i_data_deepcopy(laure_stack_t *stack, struct ArrayIData old_i_data);

void *image_deepcopy(laure_stack_t *stack, void *img) {
    if (!img) return 0;
    void *new_img_ptr = NULL;

    struct ImageHead head = read_head(img);
    
    switch (head.t) {
        case INTEGER: {
            struct IntImage *new_img = malloc(sizeof(struct IntImage));
            struct IntImage *old_img = ((struct IntImage*)img);

            new_img->t = INTEGER;

            new_img->state = old_img->state;
            new_img->datatype = old_img->datatype;

            if (old_img->state == I) {
                if (old_img->datatype == BINT) {
                    new_img->i_data = malloc(sizeof(bigint));
                    bigint_init(new_img->i_data);
                    bigint_cpy(new_img->i_data, old_img->i_data);
                }
            } else if (old_img->state == M) {
                new_img->mult = multiplicity_deepcopy(stack, old_img->mult);
            } else {
                new_img->u_data = int_domain_copy(old_img->u_data);
            }

            new_img->translator = old_img->translator;

            new_img_ptr = new_img;
            break;
        }
        case ARRAY: {
            struct ArrayImage *new_img = malloc(sizeof(struct ArrayImage));
            struct ArrayImage *old_img = ((struct ArrayImage*)img);

            new_img->t = ARRAY;
            new_img->state = old_img->state;
            new_img->arr_el = instance_deepcopy(stack, "el", old_img->arr_el);
            new_img->length_lid = old_img->length_lid;
            
            if (old_img->state == I) {
                new_img->i_data = array_i_data_deepcopy(stack, old_img->i_data);
            } else if (old_img->state == M) {
                new_img->mult = multiplicity_deepcopy(stack, old_img->mult);
            } else {
                new_img->u_data.length = int_domain_copy(old_img->u_data.length);
            }

            new_img->translator = old_img->translator;
            new_img->ref_count = old_img->ref_count;
            new_img->ref = old_img->ref;
            new_img_ptr = new_img;
            break;
        }
        case ATOM: {
            struct AtomImage *new_img = malloc(sizeof(struct AtomImage));
            struct AtomImage *old_img = ((struct AtomImage*)img);
            new_img->t = ATOM;
            new_img->unified = old_img->unified;
            new_img->translator = old_img->translator;
            
            if (old_img->unified) {
                new_img->atom = strdup(old_img->atom);
            } else {
                new_img->mult = multiplicity_deepcopy(stack, old_img->mult);
            }

            new_img_ptr = new_img;
            break;
        }
        case CONSTRAINT_FACT:
        case PREDICATE_FACT: {
            struct PredicateImage *prim = (struct PredicateImage*)img;
            if (prim->is_primitive) {
                struct PredicateImage *copy = predicate_header_new(prim->header.args, prim->header.resp, prim->t == CONSTRAINT_FACT);
                copy->is_primitive = true;
                copy->header.nestings = prim->header.nestings;
                copy->header.response_nesting = prim->header.response_nesting;
                new_img_ptr = copy;
            } else {
                new_img_ptr = img;
            }
            break;
        }
        case CHOICE: {
            new_img_ptr = img;
            break;
        }
        case STRUCTURE: {
            // new_img_ptr = laure_structure_deepcopy(stack, img);
            break;
        }
        case IMG_CUSTOM_T: {
            EnhancedImageHead ehead = read_enhanced_head(img);
            if (!ehead.copy) {
                return img;
            }
            new_img_ptr = ehead.copy(img);
            break;
        }
        case CHAR: {
            struct CharImage *old = (struct CharImage*)img;
            struct CharImage *new = malloc(sizeof(struct CharImage));
            new->t = CHAR;
            new->is_set = old->is_set;
            new->c = old->c;
            new->translator = old->translator;
            new_img_ptr = new;
            break;
        }
        default: {
            printf("not implemented deepcopy %d\n", head.t);
            new_img_ptr = img;
            break;
        };
    };

    laure_gc_track_image(new_img_ptr);
    return new_img_ptr;
}

struct PredicateImageVariationSet *pvs_new() {
    struct PredicateImageVariationSet *pvs = malloc(sizeof(struct PredicateImageVariationSet));
    pvs->set = malloc(sizeof(struct PredicateImageVariation));
    pvs->len = 0;
    pvs->finals = NULL;
    return pvs;
};

void pvs_push(struct PredicateImageVariationSet *pvs, struct PredicateImageVariation pv) {
    pvs->len++;
    pvs->set = realloc(pvs->set, sizeof(struct PredicateImageVariation) * pvs->len);
    struct PredicateImageVariation *set = pvs->set;
    set[pvs->len-1] = pv;
    pvs->set = set;
};

void predicate_addvar(
    void *img, 
    laure_expression_t *exp
) {
    struct PredicateImage *im = (struct PredicateImage*)img;

    struct PredicateImageVariation piv;
    piv.t = PREDICATE_NORMAL;
    piv.exp = exp;

    struct PredicateImageVariationSet *set = im->variations;
    pvs_push(set, piv);
};

bool not_restricted(string str);

predfinal *get_pred_final(struct PredicateImageVariation pv) {
    predfinal *pf = malloc(sizeof(predfinal));
    pf->t = PF_INTERIOR;
    
    if (pv.t == PREDICATE_NORMAL) {
        // bodied (conditions in args + conditions in body)
        laure_expression_t *expression = pv.exp;
        laure_expression_set *exp_set = expression->ba->set;

        uint body_len = expression->ba->body_len;
        uint args_len = laure_expression_get_count(expression->ba->set) - body_len;
        bool has_resp = expression->ba->has_resp;

        laure_expression_set *nset = NULL;
        laure_expression_set *complicated_nset = NULL;
        
        if (expression->ba->has_resp)
            args_len--;
        
        int pos = body_len;

        pf->interior.argc = args_len;
        pf->interior.argn = malloc(sizeof(string) * args_len);

        for (int i = 0; i < args_len; i++) {
            laure_expression_t *arg = laure_expression_set_get_by_idx(exp_set, pos);

            // changing arg names
            char argn[32];

            snprintf(argn, 32, "$%d", i);
            pf->interior.argn[i] = strdup(argn);

            if (arg->t == let_var) {
                // |  cast naming |
                // |   var to var |
                laure_expression_t *old_name_var = laure_expression_create(let_var, "", false, pf->interior.argn[i], 0, NULL);
                laure_expression_set *set = laure_expression_set_link(NULL, old_name_var);
                set = laure_expression_set_link(set, arg);
                laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, 2, 0);
                laure_expression_t *name_expression = laure_expression_create(let_name, "", false, NULL, 0, ba);
                nset = laure_expression_set_link(nset, name_expression);
            } else {
                // |  cast assert |
                // | var to value |
                laure_expression_t *var = laure_expression_create(let_var, "", false, pf->interior.argn[i], 0, NULL);
                laure_expression_set *set = laure_expression_set_link(NULL, var);
                set = laure_expression_set_link(set, arg);
                laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, 2, 0);
                laure_expression_t *assert_expression = laure_expression_create(let_assert, "", false, NULL, 0, ba);
                complicated_nset = laure_expression_set_link_branch(complicated_nset, laure_expression_compose_one(assert_expression));
            }

            pos++;
        }

        if (has_resp) {
            pf->interior.respn = strdup("$R");
            laure_expression_t *exp = laure_expression_set_get_by_idx(exp_set, pos);

            if (exp->t == let_var) {
                // cast naming
                // var to var
                laure_expression_t *old_name_var = laure_expression_create(let_var, "", false, pf->interior.respn, 0, NULL);
                laure_expression_set *set = laure_expression_set_link(NULL, old_name_var);
                set = laure_expression_set_link(set, exp);
                laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, 2, 0);
                laure_expression_t *name_expression = laure_expression_create(let_name, "", false, NULL, 0, ba);
                nset = laure_expression_set_link(nset, name_expression);
            } else {
                laure_expression_t *var = laure_expression_create(let_var, "", false, pf->interior.respn, 0, NULL);
                laure_expression_set *set = laure_expression_set_link(NULL, var);
                set = laure_expression_set_link(set, exp);
                laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, 2, 0);
                laure_expression_t *assert_expression = laure_expression_create(let_assert, "", false, NULL, 0, ba);
                complicated_nset = laure_expression_set_link_branch(complicated_nset, laure_expression_compose_one(assert_expression));
            }
        } else {
            pf->interior.respn = NULL;
        }

        nset = laure_expression_set_link_branch(nset, complicated_nset);

        laure_expression_set *last = NULL;

        for (int j = 0; j < body_len; j++) {
            last = laure_expression_set_link_branch(last, laure_expression_compose_one(laure_expression_set_get_by_idx(exp_set, j)));
        }

        laure_expression_set *body = laure_expression_set_link_branch(nset, last);
        pf->interior.body = body;
    } else if (pv.t == PREDICATE_C) {
        pf->t = PF_C;
        pf->c.pred = pv.c.pred;
        pf->c.argc = pv.c.argc;
        pf->c.hints = pv.c.hints;
        pf->c.resp_hint = pv.c.resp_hint;
    }

    return pf;
}

gen_resp generate_int(bigint* i, void *ctx_) {
    GenCtx *ctx = (GenCtx*)ctx_;
    struct IntImage *img = malloc(sizeof(struct IntImage));
    img->t = INTEGER;
    img->i_data = i;
    img->datatype = BINT;
    img->state = I;
    img->translator = new_translator(int_translator);
    return (ctx->rec)(img, ctx->external_ctx);
}

GenArrayCtx *copy_gen_ary_ctx(GenArrayCtx *old) {
    GenArrayCtx *ctx = malloc(sizeof(GenArrayCtx));
    ctx->external_ctx = old->external_ctx;
    ctx->im = old->im;
    ctx->length = old->length;
    ctx->rec = old->rec;
    ctx->aid = malloc(sizeof(struct ArrayImage));
    ctx->aid->t = old->aid->t;
    ctx->aid->state = old->aid->state;
    ctx->aid->translator = old->aid->translator;
    ctx->aid->i_data.array = malloc(sizeof(void*) * old->length);

    for (int i = 0; i < old->aid->i_data.length; i++) {
        ctx->aid->i_data.array[i] = instance_deepcopy(old->stack, "el", old->aid->i_data.array[i]);
    }
    for (int i = old->aid->i_data.length; i < old->length; i++) {
        for (int j = 0; j < old->im->ref_count; j++) {
            ref_element ref = old->im->ref[j];
            if (i == ref.idx) {
                ctx->aid->i_data.array[i] = old->aid->i_data.array[i];
                break;
            }
        }
    }
    ctx->aid->arr_el = old->aid->arr_el;
    ctx->aid->i_data.length = old->aid->i_data.length;
    return ctx;
}

string convert_char_array(struct ArrayIData char_arr);

gen_resp generate_array_tail(void *img, void *ctx_) {
    GenArrayCtx *old_ctx = (GenArrayCtx*)ctx_;
    GenArrayCtx *ctx = copy_gen_ary_ctx(old_ctx);

    if (! ctx->aid->i_data.array[ctx->aid->i_data.length])
        ctx->aid->i_data.array[ctx->aid->i_data.length] = instance_new("el", "", img);
    else {
        ctx->aid->i_data.array[ctx->aid->i_data.length]->image = img; 
    }

    ctx->aid->i_data.length++;

    if (ctx->length == ctx->aid->i_data.length) {
        void *copy = image_deepcopy(ctx->stack, ctx->aid);
        gen_resp gr = ctx->rec(copy, ctx->external_ctx);
        ctx->aid->i_data.length--;
        return gr;
    } else {
        return image_generate(ctx->stack, get_image(ctx->im->arr_el), generate_array_tail, ctx);
    }
}

bool img_equals(void* img1, void* img2);

gen_resp generate_array(bigint *length_, void *ctx_) {
    GenCtx *ctx = (GenCtx*)ctx_;

    struct ArrayImage *arr_im = (struct ArrayImage*)ctx->im;

    if (arr_im->length_lid) {
        struct IntImage *img;
        Cell cell = laure_stack_get_cell_by_lid(ctx->stack, arr_im->length_lid, true);
        if (cell.instance) {
            if (! ctx->im2) {
                ctx->im2 = image_deepcopy(ctx->stack, cell.instance->image);
            }
            img = cell.instance->image;

            if (! int_check(ctx->im2, length_)) {
                gen_resp gr; 
                gr.r = true; 
                gr.qr = respond(q_false, 0);
                return gr;
            }

            img->state = I;
            img->i_data = length_;
        }
    }

    int length = (int)bigint_double(length_);
    if (length == 0) {
        return ctx->rec(array_i_new(NULL, 0), ctx->external_ctx);
    }

    struct ArrayImage *ary_im = (struct ArrayImage*)ctx->im;

    GenArrayCtx *gen_ary_ctx = malloc(sizeof(GenArrayCtx));
    void **aid_data = malloc(sizeof(void*) * length);
    memset(aid_data, 0, length * sizeof(void*));

    for (int i = 0; i < ary_im->ref_count; i++) {
        ref_element ref = ary_im->ref[i];
        Cell cell = laure_stack_get_cell_by_lid(ctx->stack, ref.link_id, false);
        aid_data[ref.idx] = cell.instance;
    }

    gen_ary_ctx->aid = array_i_new(aid_data, 0);
    gen_ary_ctx->aid->arr_el = arr_im->arr_el;
    gen_ary_ctx->aid->i_data.length = 0;
    gen_ary_ctx->length = length;
    gen_ary_ctx->im = ary_im;
    gen_ary_ctx->external_ctx = ctx->external_ctx;
    gen_ary_ctx->rec = ctx->rec;
    gen_ary_ctx->stack = ctx->stack;

    return image_generate(ctx->stack, get_image(gen_ary_ctx->im->arr_el), generate_array_tail, gen_ary_ctx);
}

bool predicate_search_check(Instance *chd, struct PredicateImage *prim) {
    if (read_head(chd->image).t != prim->t) return false;
    else if (str_starts(chd->name, "__")) return false;
    struct PredicateImage *nonprim = (struct PredicateImage*) chd->image;
    if (nonprim->is_primitive) return false;
    else if (nonprim->variations->set[0].t == PREDICATE_C) return false;
    
    if (
        (prim->header.args->len != nonprim->header.args->len) ||
        (prim->header.resp != nonprim->header.resp) || 
        (prim->header.response_nesting != nonprim->header.response_nesting)
    ) return false;

    for (uint idx = 0; idx < prim->header.args->len; idx++) {
        Instance *prim_arg = prim->header.args->data[idx];
        Instance *nonprim_arg = nonprim->header.args->data[idx];
        //! todo smart check
        if (read_head(prim_arg->image).t != read_head(nonprim_arg->image).t) return false;
    }

    if (
        prim->header.resp &&
        read_head(prim->header.resp->image).t 
        != read_head(nonprim->header.resp->image).t
    ) { 
        return false;
    }
    return true;
}

struct unify_array_ctx {
    gen_resp (*rec)(void*, void*);
    void *external_ctx;
    laure_stack_t *stack;
    Instance *element;
    uint idx;
    struct ArrayImage *im;
};

gen_resp unify_array(void *img, struct unify_array_ctx *ctx) {
    struct ArrayImage *arr = image_deepcopy(ctx->stack, ctx->im);
    arr->i_data.array[ctx->idx]->image = img; 

    for (int i = ctx->idx + 1; i < ctx->im->i_data.length; i++) {
        struct unify_array_ctx *uctx = malloc(sizeof(struct unify_array_ctx));
        uctx->rec = ctx->rec;
        uctx->idx = i;
        uctx->im = arr;
        uctx->external_ctx = ctx->external_ctx;
        uctx->element = arr->i_data.array[i];
        uctx->stack = ctx->stack;
        gen_resp gr = image_generate(ctx->stack, uctx->element->image, unify_array, uctx);
        free(uctx);
        return gr;
    }
    ctx->im = arr;
    return ctx->rec(ctx->im, ctx->external_ctx);
};

struct MultGenCtx {
    gen_resp (*rec)(void*, void*);
    void* context;
    ;
    laure_stack_t *stack;
};

gen_resp multiplicity_generate(multiplicity *mult, gen_resp (*rec)(void*, void*), void* context) {
    for (int i = 0; i < mult->amount; i++) {
        gen_resp gr = rec(mult->members[i], context);
        if (gr.r != 1) return gr;
    }
    gen_resp gr = {1, respond(q_yield, NULL)};
    return gr;
}

gen_resp image_generate(laure_stack_t *stack, void* img, gen_resp (*rec)(void*, void*), void* external_ctx) {
    assert(img != NULL);
    struct ImageHead head = read_head(img);
    img = image_deepcopy(stack, img);

    switch (head.t) {
        case INTEGER: {
            struct IntImage *im = (struct IntImage*)img;
            if (im->state == I) {
                // integer has only one value
                return rec(im, external_ctx);
            } else if (im->state == M) {
                return multiplicity_generate(im->mult, rec, external_ctx);
            } else {
                // choicepoint
                GenCtx *gctx = malloc(sizeof(GenCtx));
                gctx->stack = stack;
                gctx->external_ctx = external_ctx;
                gctx->rec = rec;
                return int_domain_generate(im->u_data, generate_int, gctx);
            }
            break;
        }
        case ARRAY: {
            struct ArrayImage *im = (struct ArrayImage*)img;
            if (im->state == I) {
                for (int i = 0; i < im->i_data.length; i++) {
                    Instance *el = im->i_data.array[i];

                    struct unify_array_ctx *uctx = malloc(sizeof(struct unify_array_ctx));
                    uctx->rec = rec;
                    uctx->stack = stack;
                    uctx->element = el;
                    uctx->external_ctx = external_ctx;
                    uctx->idx = i;
                    uctx->im = im;

                    gen_resp gr = image_generate(stack, el->image, unify_array, uctx);
                    free(uctx);
                    return gr;
                }
                return rec(im, external_ctx);
            } else if (im->state == M) {
                return multiplicity_generate(im->mult, rec, external_ctx);
            } else {
                // generating length
                GenCtx *gctx = malloc(sizeof(GenCtx));
                gctx->external_ctx = external_ctx;
                gctx->gac = NULL;
                gctx->stack = stack;
                gctx->im = im;
                gctx->rec = rec;
                gctx->im2 = 0;
                return int_domain_generate(im->u_data.length, generate_array, gctx);
            }
            break;
        }
        case ATOM: {
            struct AtomImage *im = (struct AtomImage*)img;
            if (im->unified) {
                return rec(im, external_ctx);
            } else {
                return multiplicity_generate(im->mult, rec, external_ctx);
            }
            break;
        }
        case STRUCTURE: {
            // return laure_structure_generate(stack, (struct StructureImage*)img, rec, external_ctx);
            gen_resp gr;
            gr.r = true;
            return gr;
        }
        case IMG_CUSTOM_T:  {
            EnhancedImageHead head = read_enhanced_head(img);
            if (head.is_instantiated && head.is_instantiated(img)) {
                return rec(img, external_ctx);
            }
            break;
        }
        case CONSTRAINT_FACT:
        case PREDICATE_FACT: {
            struct PredicateImage *im = (struct PredicateImage*)img;
            if (im->is_primitive) {
                // predicate search
                Cell cell;
                STACK_ITER(stack, cell, {
                    if (predicate_search_check(cell.instance, im)) {
                        rec(cell.instance->image, external_ctx);
                    }
                }, false);
                STACK_ITER(stack->global, cell, {
                    if (predicate_search_check(cell.instance, im)) {
                        rec(cell.instance->image, external_ctx);
                    }
                }, false);
            } else {
                return rec(im, external_ctx);
            }
            break;
        }
        default: {
            gen_resp gr;
            gr.qr = respond(q_error, strdup("cannot instantiate"));
            gr.r = false;
            return gr;
        }
    }
    gen_resp gr;
    gr.r = true;
    return gr;
};

struct PredicateCImageHint *hint_new(string name, Instance* instance) {
    struct PredicateCImageHint *hint = malloc(sizeof(struct PredicateCImageHint));
    hint->name = name;
    hint->hint = instance;
    return hint;
}

choice_img *choice_img_new() {
    choice_img *cimg = malloc(sizeof(choice_img));
    cimg->t = CHOICE;
    cimg->multiplicity = malloc(sizeof(Instance*));
    cimg->length = 0;
    cimg->translator = NULL;
    return cimg;
};

void laure_ensure_bigint(struct IntImage* img) {
    
    if (img->state != I) return;
    switch (img->datatype) {
        case BINT: return;
        default: break;
    }
    img->datatype = BINT;
}

multiplicity *multiplicity_create() {
    multiplicity *mult = malloc(sizeof(multiplicity));
    mult->amount = 0;
    mult->capacity = MULTIPLICITY_CAPACITY;
    mult->members = malloc(sizeof(void*) * mult->capacity);
    memset(mult->members, 0, sizeof(void*) * mult->capacity);
    return mult;
}

multiplicity *multiplicity_deepcopy(laure_stack_t *stack, multiplicity *mult) {
    if (mult == NULL) return NULL;
    multiplicity *new = malloc(sizeof(multiplicity));
    new->amount = mult->amount;
    new->capacity = mult->amount;
    new->members = malloc(sizeof(void*) * mult->amount);
    for (int i = 0; i < mult->amount; i++) {
        new->members[i] = image_deepcopy(stack, mult->members[i]);
    }
    return new;
}

void multiplicity_insert(multiplicity *mult, void *img) {
    if (mult->amount + 1 > mult->capacity) {
        mult->capacity = mult->capacity * 2;
        mult->members = realloc(mult->members, sizeof(void*) * mult->capacity);
    }
    mult->members[mult->amount] = img;
    mult->amount++;
}

void multiplicity_free(multiplicity *mult) {
    free(mult->members);
    free(mult);
}

string default_repr(Instance *ins) {
    return strdup(ins->name);
}

string predicate_repr(Instance *ins) {
    char buff[128];

    char argsbuff[64] = {0};
    char respbuff[64] = {0};

    struct PredicateImage *img = (struct PredicateImage*)ins->image;

    for (int i = 0; i < img->header.args->len; i++) {
        string argn = img->header.args->data[i]->name;
        if (i != 0) {
            strcat(argsbuff, ", ");
            strcat(argsbuff, argn);
        } else strcpy(argsbuff, argn);
    }

    if (img->header.resp != NULL) {
        snprintf(respbuff, 64, " -> %s", img->header.resp->name);
    } else
        respbuff[0] = 0;

    string name = ins->name;
    if (img->is_primitive) name = "\0";
    
    snprintf(buff, 128, "(?%s(%s)%s)", name, argsbuff, respbuff);
    return strdup(buff);
}

string constraint_repr(Instance *ins) {
    char buff[128];

    char argsbuff[64];
    char respbuff[64];

    struct PredicateImage *img = (struct PredicateImage*)ins->image;

    for (int i = 0; i < img->header.args->len; i++) {
        string argn = img->header.args->data[i]->name;
        if (i != 0) {
            strcat(argsbuff, ", ");
            strcat(argsbuff, argn);
        } else strcpy(argsbuff, argn);
    }

    if (img->header.resp != NULL)
        snprintf(respbuff, 64, " -> %s", img->header.resp->name);
    else
        respbuff[0] = 0;
    
    snprintf(buff, 128, "(#%s(%s)%s)", ins->name, argsbuff, respbuff);
    return strdup(buff);
}

string choice_repr(Instance *ins) {
    /*
    string buff = "(unbinded ";
    choice_img *img = (choice_img*)ins->image;

    for (int i = 0; i < img->length; i++) {
        Instance *part = img->multiplicity[i];
        string repr = part->repr(part);
        string tmp = strdup(buff);
        if (i == 0) {
            ALLOC_SPEC_STRING(buff, "%s%s%s%s", tmp, GRAY_COLOR, repr, NO_COLOR);
        } else {
            ALLOC_SPEC_STRING(buff, "%s | %s%s%s", tmp, GRAY_COLOR, repr, NO_COLOR);
        }
    }*/
    return strdup("todo repr");
}

string char_repr(Instance *ins) {
    struct CharImage *img = (struct CharImage*)ins->image;
    if (img->is_set) {
        char buff[10];
        laure_string_put_char(buff, img->c);
        char buff2[20];
        snprintf(buff2, 20, "'%s'", buff);
        return strdup( buff2 );
    } else {
        return strdup( "(char)" );
    }
}

int convert(int c) {
    switch (c) {
        case '\a': return 'a';
        case '\b': return 'b';
        case '\e': return 'e';
        case '\f': return 'f';
        case '\n': return 'n';
        case '\r': return 'r';
        case '\t': return 't';
        case '\v': return 'v';
        default: return -1;
    }
}

string string_repr(Instance *ins) {
    struct ArrayImage *arr = (struct ArrayImage*)ins->image;
    if (arr->state == I) {
        char buff[512];
        strcpy(buff, "\"");
        for (int idx = 0; idx < arr->i_data.length; idx++) {
            int c = ((struct CharImage*)arr->i_data.array[idx]->image)->c;
            int cd = convert(c);
            char mbuff[5];
            if (cd != -1) {
                strcat(buff, "\\");
                c = cd;
            }
            laure_string_put_char(mbuff, c);
            strcat(buff, mbuff);
        }
        strcat(buff, "\"");
        return strdup( buff );
    } else {
        return strdup( "(string)" );
    }
}

string array_repr(Instance *ins) {
    struct ArrayImage *img = (struct ArrayImage*)ins->image;
    char buff[512];
    if (img->state == I) {
        strcpy(buff, "[");
        for (int i = 0; i < img->i_data.length; i++) {
            Instance *el = img->i_data.array[i];
            string repr = img->arr_el->repr(el);
            strcat(buff, repr);
            free(repr);
            if (i != img->i_data.length - 1)
                strcat(buff, ", ");
        }
        strcat(buff, "]");
    } else if (img->state == U) {
        string arr_el_repr = img->arr_el->repr(img->arr_el);
        string length_repr = int_domain_repr(img->u_data.length);
        snprintf(buff, 512, "[%s, ...] of length %s", arr_el_repr, length_repr);
        free(arr_el_repr);
        free(length_repr);
    }
    return strdup(buff);
}

Instance *instance_new(string name, string doc, void *image) {
    Instance *ins = malloc(sizeof(Instance));
    ins->name = name;
    ins->derived = NULL;
    ins->doc = doc;
    ins->repr = default_repr;
    ins->locked = false;
    ins->image = image;
    laure_gc_track_instance(ins);
    laure_gc_track_image(image);
    return ins;
};

Instance *choice_instance_new(string name, string doc) {
    Instance *ins = instance_new(name, doc, choice_img_new());
    ins->repr = choice_repr;
    return ins;
}

struct InstanceSet *instance_set_new() {
    struct InstanceSet *is = malloc(sizeof(struct InstanceSet));
    is->data = malloc(sizeof(Instance*));
    is->len = 0;
    return is;
};

void instance_set_push(struct InstanceSet* ins_set, Instance *ins) {
    ins_set->len++;
    ins_set->data = realloc(ins_set->data, sizeof(Instance*) * ins_set->len);
    Instance** set = ins_set->data;
    set[ins_set->len - 1] = ins;
    ins_set->data = set;
};

Instance *instance_deepcopy(laure_stack_t *stack, string name, Instance *from_instance) {
    if (from_instance == NULL) return from_instance;
    Instance *instance = malloc(sizeof(Instance));
    instance->derived = from_instance;
    instance->doc = from_instance->doc;
    instance->image = image_deepcopy(stack, from_instance->image);
    instance->locked = false;
    instance->name = name;
    instance->repr = from_instance->repr;
    laure_gc_track_instance(instance);
    return instance;
};

Instance *instance_deepcopy_with_image(laure_stack_t *stack, string name, Instance *from_instance, void *image) {
    Instance *instance = instance_deepcopy(stack, name, from_instance);
    instance->image = image_deepcopy(stack, image);
    return instance;
};

string instance_get_doc(Instance *ins) {
    return ins->doc;
}

Instance *instance_shallow_copy(string name, Instance *from_instance) {
    Instance *instance = malloc(sizeof(Instance));
    instance->derived = from_instance;
    instance->doc = from_instance->doc;
    instance->image = from_instance->image;
    instance->locked = false;
    instance->name = strdup(name);
    instance->repr = from_instance->repr;
    return instance;
};

preddata *preddata_new(laure_stack_t *stack) {
    preddata *pd = malloc(sizeof(preddata));
    pd->argc = 0;
    pd->resp = NULL;
    pd->argv = malloc(sizeof(struct predicate_arg));
    pd->stack = stack;
    return pd;
};

void preddata_push(preddata *pd, struct predicate_arg pa) {
    pd->argc++;
    pd->argv = realloc(pd->argv, sizeof(struct predicate_arg) * pd->argc);
    struct predicate_arg *pas = pd->argv;
    pas[pd->argc-1] = pa;
};

bool instantiated_or_mult(Instance *ins) {
    struct ImageHead head = read_head(ins->image);
    switch (head.t) {
        case INTEGER:
            return ((struct IntImage*)ins->image)->state == I || ((struct IntImage*)ins->image)->state == M;
        case ARRAY:
            return ((struct ArrayImage*)ins->image)->state == I || ((struct ArrayImage*)ins->image)->state == M;
        case ATOM:
            return true;
        case PREDICATE_FACT:
            return true;
        case STRUCTURE:
            /* return laure_structure_instantiated(ins->image); */ return 1;
        case CHOICE:
            return true;
    }
}

bool instantiated(Instance *ins) {
    if (!ins->image) return false;

    struct ImageHead head = read_head(ins->image);
    switch (head.t) {
        case INTEGER:
            return ((struct IntImage*)ins->image)->state == I;
        case ARRAY:
            return ((struct ArrayImage*)ins->image)->state == I;
        case ATOM:
            return ((struct AtomImage*)ins->image)->unified;
        case PREDICATE_FACT:
            return true;
        case STRUCTURE:
            /* return laure_structure_instantiated(ins->image); */ return 1;
        case CHOICE:
            return true;
        case CHAR:
            return ((struct CharImage*)ins->image)->is_set;
    }
};

struct Translator *new_translator(bool (*invoke)(string, void*, laure_stack_t*)) {
    struct Translator *tr = malloc(sizeof(struct Translator));
    tr->invoke = invoke;
    return tr;
};

multiplicity *translate_to_multiplicity(
    laure_expression_t *exp, 
    bool (*transl) (laure_expression_t*, void*, laure_stack_t*, bool),
    void *rimg,
    laure_stack_t *stack
) {
    multiplicity *mult = multiplicity_create();
    printf("todo multiplicity create\n");
    return mult;
}

// translators
bool int_translator(laure_expression_t *exp, void* rimg, laure_stack_t *stack) {
    if (exp->t != let_custom) return false;
    string raw = exp->s;
    struct IntImage *img = (struct IntImage*)rimg;

    string _temp = strstr(exp->s, "..");

    if (_temp) {
        // Domain declared
        bool left_secl = false;
        bool left_minus = false;
        bool right_secl = false;
        bool right_minus = false;

        if (exp->s[0] == '<') {
            left_secl = true;
            exp->s++;
        }

        if (exp->s[0] == '-') {
            left_minus = true;
            exp->s++;
        }

        string left = malloc(strlen(exp->s) - strlen(_temp));
        strncpy(left, exp->s, strlen(exp->s) - strlen(_temp));

        _temp = _temp + 2;

        uint right_n = strlen(_temp);

        if (lastc(exp->s) == '>') {
            right_secl = true;
            right_n--;
        }

        if (_temp[0] == '-') {
            right_minus = true;
            _temp++;
            right_n--;
        }

        string right = malloc(right_n);
        strncpy(right, _temp, right_n);

        for (int i = 0; i < strlen(left); i++) 
            if (!char_isnumber(left[i])) {
                free(left); free(right);
                return false;
            };
        
        for (int i = 0; i < strlen(right); i++) 
            if (!char_isnumber(right[i])) {
                free(left); free(right);
                return false;
            };

        Domain *new_domain = int_domain_new();

        if (strlen(left)) {
            bigint *bi_left = malloc(sizeof(bigint));
            bigint_init(bi_left);
            bigint_from_str(bi_left, left);
            if (left_minus) bigint_negate(bi_left);
            IntValue left_value = {left_secl ? SECLUDED : INCLUDED, bi_left};
            int_domain_gt(new_domain, left_value);
        }

        if (strlen(right)) {
            bigint *bi_right = malloc(sizeof(bigint));
            bigint_init(bi_right);
            bigint_from_str(bi_right, right);
            if (right_minus) bigint_negate(bi_right);
            IntValue right_value = {right_secl ? SECLUDED : INCLUDED, bi_right};
            int_domain_lt(new_domain, right_value);
        }

        img->t = U;
        img->u_data = new_domain;
        return true;
    }

    bool minus = false;
    if (raw[0] == '-') {minus = true; raw++;}
    else if (raw[0] == '+') raw++;

    for (int i = 0; i < strlen(raw); i++) 
        if (!char_isnumber(raw[i])) return false;
    
    bigint *bi = malloc(sizeof(bigint));
    bigint_init(bi);
    bigint_from_str(bi, raw);

    if (minus) bigint_negate(bi);

    if (img->state == U) {
        bool result = int_domain_check(img->u_data, bi);
        if (!result) {
            free(bi->words);
            free(bi);
            return false;
        }
        int_domain_free(img->u_data);
    } else if (img->state == M) {
        bool found = false;

        for (int i = 0; i < img->mult->amount; i++) {
            struct IntImage *member_img = (struct IntImage*) img->mult->members[i];
            if (bigint_cmp(member_img->i_data, bi)) {
                found = true;
                break;
            }
        }

        if (!found) {
            free(bi->words);
            free(bi);
            return false;
        } else {
            multiplicity_free(img->mult);
        }
    } else {
        bool result = (bigint_cmp(img->i_data, bi) == 0);
        free(bi->words);
        free(bi);
        return result;
    }

    img->state = I;
    img->i_data = bi;
    return true;
}

bool atom_translator(laure_expression_t *exp, void* rimg, laure_stack_t *stack) {
    printf("translate atom\n");
    return true;
}

void* mul_eq(multiplicity *mult, void* img) {
    for (int i = 0; i < mult->amount; i++) {
        if (img_equals(mult->members[i], img)) {
            return mult->members[i];
        }
    }
    return NULL;
}


bool char_translator(laure_expression_t *exp, void* rimg, laure_stack_t *stack) {
    if (exp->t != let_custom) return false;
    else if (strlen(exp->s) < 3 || exp->s[0] != '\'' || lastc(exp->s) != '\'') return false;
    struct CharImage *img = (struct CharImage*)rimg;

    string cs = malloc(strlen(exp->s) - 1);
    strncpy(cs, exp->s + 1, strlen(exp->s) - 2);
    int c = laure_string_get_char(&cs);
    
    if (img->is_set) {
        return c == img->c;
    } else {
        img->c = c;
        img->is_set = true;
    }
    return true;
}

bool array_translator(laure_expression_t *exp, void* rimg, laure_stack_t *stack) {
    struct ArrayImage *array = (struct ArrayImage*)rimg;
    if (array->t != ARRAY || exp->t != let_array) return false;

    if (array->state == U) {
        uint length = laure_expression_get_count(exp->ba->set);
        Instance **ptrs = malloc(sizeof(void*) * length);

        for (int i = 0; i < length; i++) {
            laure_expression_t *el_exp = laure_expression_set_get_by_idx(exp->ba->set, i);
            void *img;
            if (el_exp->t == let_var) {
                Instance *el = laure_stack_get(stack, el_exp->s);
                if (! el) {
                    return false;
                }
                img = el->image;
            } else {
                img = image_deepcopy(stack, array->arr_el->image);

                bool result = read_head(array->arr_el->image).translator->invoke(el_exp, img, stack);
                if (! result) {
                    return false;
                }
            }
            ptrs[i] = instance_new(strdup("element"), NULL, img);
        }
        array->i_data.length = length;
        array->i_data.array = ptrs;
        array->state = I;
    } else {
        if (array->i_data.length != exp->ba->body_len) return false;
        for (int idx = 0; idx < array->i_data.length; idx++) {
            Instance *el = array->i_data.array[idx];
            bool res = read_head(el->image).translator->invoke(laure_expression_set_get_by_idx(exp->ba->set, idx), el->image, stack);
            if (! res) return false;
        }
    }
    return true;
}

char convert_escaped_char(char c) {
    switch (c) {
        case 'a': return '\a';
        case 'b': return '\b';
        case 'e': return '\e';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        default: return c;
    }
}

bool string_translator(laure_expression_t *exp, void *rimg, laure_stack_t *stack) {
    if (exp->t != let_custom || strlen(exp->s) < 2 || exp->s[0] != '"' || lastc(exp->s) != '"') return false;
    struct ArrayImage *arr_img = (struct ArrayImage*)rimg;
    if (arr_img->state == U) {
        string str = malloc(strlen(exp->s) - 1);
        strncpy(str, exp->s + 1, strlen(exp->s) - 2);
        uint len = laure_string_strlen(str);
        Instance **array = malloc(sizeof(void*) * len);

        bool escaped = false;
        uint i = 0;
        for (int idx = 0; idx < len; idx++) {
            int c = laure_string_char_at_pos(str, strlen(str), idx);

            if (escaped) {
                c = convert_escaped_char(c);
                escaped = false;
            } else if ( c == '\\') {
                escaped = true;
                continue;
            }

            struct CharImage *c_img = malloc(sizeof(struct CharImage));
            c_img->translator = read_head(laure_stack_get(stack, "char")->image).translator;
            c_img->t = CHAR;
            c_img->is_set = true;
            c_img->c = c;
            
            Instance *c_ins = instance_new(strdup("el"), NULL, c_img);
            array[i] = c_ins;
            i++;
        }
        arr_img->state = I;
        arr_img->i_data.array = array;
        arr_img->i_data.length = i;
    } else {
        if ((laure_string_strlen(exp->s) - 2) != arr_img->i_data.length) return false;
        for (int idx = 0; idx < arr_img->i_data.length; idx++) {
            int c = ((struct CharImage*)arr_img->i_data.array[idx]->image)->c;
            int c2 = laure_string_char_at_pos(exp->s, strlen(exp->s), idx + 1);
            if (c != c2) return false;
        }
    }
    return true;
}


bool int_check(void *img_, void *bi_) {
    bigint *bi = (bigint*)bi_;
    struct IntImage *img = (struct IntImage*)img_;
    if (img->state == I) {
        return bigint_cmp(img->i_data, bi) == 0;
    } else if (img->state == M) {
        for (int i = 0; i < img->mult->amount; i++) {
            if (bigint_cmp(((struct IntImage*)img->mult->members[i])->i_data, bi) == 0) return 1;
        }
        return 0;
    } else {
        return int_domain_check(img->u_data, bi);
    }
}

bool img_equals(void* img1, void* img2) {

    if (!img1 || !img2) return true;

    struct ImageHead head1 = read_head(img1);
    struct ImageHead head2 = read_head(img2);

    if (head1.t != head2.t) return false;

    switch (head1.t) {
        case INTEGER: {

            struct IntImage *img1_t = (struct IntImage*)img1;
            struct IntImage *img2_t = (struct IntImage*)img2;
            
            if (img1_t->state == I && img2_t->state == I) {

                return bigint_cmp(((struct IntImage*)img1)->i_data, ((struct IntImage*)img2)->i_data) == 0;

            } else if (img1_t->state == M && img2_t->state == M) {
                if (img1_t->mult->amount != img2_t->mult->amount) return false;

                for (int i = 0; i < img1_t->mult->amount; i++) {
                    bool ok = false;
                    for (int j = 0; j < img2_t->mult->amount; j++) {
                        if (img_equals(img1_t->mult->members[i], img2_t->mult->members[j])) {
                            ok = true;
                            break;
                        }
                    }
                    if (!ok) return false;
                }
                return true;

            } else if (img1_t->state == M && img2_t->state == I) {
                struct IntImage *s = mul_eq(img1_t->mult, img2_t);
                if (s == NULL) return 0;
                *img1_t = *s;
                return 1;
                
            } else if (img1_t->state == I && img2_t->state == M) {
                struct IntImage *s = mul_eq(img2_t->mult, img1_t);
                if (s == NULL) return 0;
                *img2_t = *s;
                // fix this shiity freeing torture
                return 1;

            } else {
                if (img1_t->state == I) {
                    if (! int_domain_check(img2_t->u_data, img1_t->i_data)) return false;
                    
                    int_domain_free(img2_t->u_data);
                    img2_t->i_data = bigint_copy(img1_t->i_data);
                    img2_t->state = I;
                } else if (img2_t->state == I) {
                    if (! int_domain_check(img1_t->u_data, img2_t->i_data)) return false;
                    
                    int_domain_free(img1_t->u_data);
                    img1_t->i_data = bigint_copy(img2_t->i_data);
                    img1_t->state = I;
                }
                return true;
            }
        }
        case ARRAY: {
            //! fixme
            struct ArrayImage *img1_t = (struct ArrayImage*)img1;
            struct ArrayImage *img2_t = (struct ArrayImage*)img2;

            //! todo add typecheck

            if (img1_t->state == I && img2_t->state == I) {
                if (((struct ArrayImage*)img1)->i_data.length != ((struct ArrayImage*)img2)->i_data.length) return false;

                for (int i = 0; i < ((struct ArrayImage*)img1)->i_data.length; i++) {
                    if (!img_equals(((struct ArrayImage*)img1)->i_data.array[i]->image, ((struct ArrayImage*)img2)->i_data.array[i]->image)) return false;
                }

                return true;
            } else if (img1_t->state == I || img2_t->state == I) {
                struct ArrayImage *from;
                struct ArrayImage *to;

                if (img1_t->state == I) {
                    from = img1_t;
                    to = img2_t;
                } else {
                    from = img2_t;
                    to = img1_t;
                }

                bigint *bi_len = bigint_create(from->i_data.length);

                if (! int_domain_check(to->u_data.length, bi_len)) {bigint_free(bi_len); return false;}
                bigint_free(bi_len);
                int_domain_free(to->u_data.length);
                to->i_data.length = from->i_data.length;
                to->i_data.array = from->i_data.array;
                to->state = I;
                return true;
            } else {
                printf("both arrays are amguative, todo\n");
                return true;
            }
        }
        case ATOM: {
            struct AtomImage *img1_t = (struct AtomImage*)img1;
            struct AtomImage *img2_t = (struct AtomImage*)img2;
            if (img1_t->unified && img2_t->unified) {
                return str_eq(img1_t->atom, img2_t->atom);

            } else if (img1_t->unified && !img2_t->unified) {
                struct AtomImage *s = mul_eq(img2_t->mult, img1_t);
                if (s == NULL) return 0;
                img2_t->unified = true;
                img2_t->atom = s->atom;

            } else if (!img1_t->unified && img2_t->unified) {
                struct AtomImage *s = mul_eq(img1_t->mult, img2_t);
                if (s == NULL) return 0;
                img1_t->unified = true;
                img1_t->atom = s->atom;

            } else {
                if (img1_t->mult->amount != img2_t->mult->amount) return false;

                for (int i = 0; i < img1_t->mult->amount; i++) {
                    bool ok = false;
                    for (int j = 0; j < img2_t->mult->amount; j++) {
                        if (img_equals(img1_t->mult->members[i], img2_t->mult->members[j])) {
                            ok = true;
                            break;
                        }
                    }
                    if (!ok) return false;
                }
                return true;
            }
            break;
        }
        case STRUCTURE: {
            // return laure_structures_eq(img1, img2);
            return 0;
        }
        case CHAR: {
            struct CharImage *c1 = (struct CharImage*)img1;
            struct CharImage *c2 = (struct CharImage*)img2;
            if (c1->is_set && c2->is_set) {
                return c1->c == c2->c;
            } else if (c1->is_set) {
                c2->c = c1->c;
                c2->is_set = true;
            } else if (c2->is_set) {
                c1->c = c2->c;
                c1->is_set = true;
            }
            return true;
        }
        case IMG_CUSTOM_T: {
            EnhancedImageHead head = read_enhanced_head(img1);
            EnhancedImageHead head2 = read_enhanced_head(img2);
            if (strcmp(head.identifier, head2.identifier) != 0) return false;
            return head.eq && head.eq(img1, img2);
        }
        case PREDICATE_FACT:
        case CONSTRAINT_FACT: {
            struct PredicateImage *p1 = (struct PredicateImage*)img1;
            struct PredicateImage *p2 = (struct PredicateImage*)img2;
            if (! (p1->is_primitive || p2->is_primitive)) return false;
            else if (p1->is_primitive && p2->is_primitive) {
                printf("cmp primitives not implemented\n");
                return false;
            }
            struct PredicateImage *prim, *nonprim;
            if (p1->is_primitive) {
                prim = p1; nonprim = p2;
            } else {
                prim = p2; nonprim = p1;
            }
            
            if (
                (prim->header.args->len != nonprim->header.args->len) ||
                (prim->header.resp != nonprim->header.resp) || 
                (prim->header.response_nesting != nonprim->header.response_nesting)
            ) return false;

            for (uint idx = 0; idx < prim->header.args->len; idx++) {
                Instance *prim_arg = prim->header.args->data[idx];
                Instance *nonprim_arg = nonprim->header.args->data[idx];
                //! todo smart check
                if (read_head(prim_arg->image).t != read_head(nonprim_arg->image).t) return false;
            }

            if (
                prim->header.resp &&
                read_head(prim->header.resp->image).t 
                != read_head(nonprim->header.resp->image).t
            ) { 
                return false;
            }

            *prim = *nonprim;
            return true;
        }
    }
}

string atom_repr(Instance* ins) {
    struct AtomImage *img = (struct AtomImage*)ins->image;

    if (img->unified) {
        return strdup(img->atom);
    } else {
        char buffer[64];
        memset(buffer, '\0', 64);
        strcpy(buffer, "@{");
        for (int i = 0; i < img->mult->amount; i++) {
            strcat(buffer, ((struct AtomImage*)img->mult->members[i])->atom);
            if (i != img->mult->amount - 1) strcat(buffer, ", ");
        }
        strcat(buffer, "}");
        return strdup(buffer);
    }
}

bool is_array_of_int(Instance* instance) {
    if (((struct ArrayImage*)instance->image)->t != ARRAY) return false;
    return (bool) read_head(((struct ArrayImage*)instance->image)->arr_el->image).t == INTEGER;
}

bool is_int(Instance* instance) {
    if (!instance->image) return false;
    return (bool) ((struct IntImage*)instance->image)->t == INTEGER;
}

qresp image_control(void *inst_img, control_ctx* ctx) {
    Instance* ins = (Instance*)ctx->data;
    //! eval(ctx);
    return RESPOND_YIELD;
}

control_ctx *create_control_ctx(laure_stack_t *stack, qcontext* qctx, var_process_kit* vpk, void* data, bool no_ambig) {
    control_ctx *ptr = malloc(sizeof(control_ctx));
    ptr->stack = stack;
    ptr->data = data;
    ptr->qctx = qctx;
    ptr->vpk = vpk;
    ptr->no_ambig = no_ambig;
    return ptr;
}

void choice_img_add(choice_img *cimg, Instance *n) {
    for (int i = 0; i < cimg->length; i++) {
        if (img_equals(n->image, cimg->multiplicity[i]->image)) return;
    }
    cimg->length++;
    cimg->multiplicity = realloc(cimg->multiplicity, sizeof(Instance*) * cimg->length);
    cimg->multiplicity[cimg->length-1] = n;
};

struct ArrayIData array_i_data_deepcopy(laure_stack_t *stack, struct ArrayIData old_i_data) {
    struct ArrayIData i_data;
    i_data.length = old_i_data.length;
    i_data.array = malloc(sizeof(Instance*) * i_data.length);
    for (int i = 0; i < i_data.length; i++) {
        i_data.array[i] = instance_deepcopy(stack, strdup(old_i_data.array[i]->name), old_i_data.array[i]);
    }
    return i_data;
}

size_t instance_get_size_deep(Instance *instance) {
    if (instance == NULL) return 0;
    
    size_t sz = 0;
    sz = sz + sizeof(instance);
    sz = sz + (sizeof(char) * strlen(instance->name));
    sz = sz + (sizeof(char) * strlen(instance->doc));
    sz = sz + image_get_size_deep(instance->image);
    return sz;
}

size_t image_get_size_deep(void *image) {
    if (image == NULL) return 0;

    size_t sz = 0;
    struct ImageHead head = read_head(image);
    switch (head.t) {
        case INTEGER: {
            sz = sz + sizeof(struct IntImage);
            struct IntImage *img = (struct IntImage*)image;
            if (img->state == I) {
                if (img->datatype == BINT) {
                    sz = sz + (img->i_data->capacity * sizeof(*img->i_data->words));
                    sz = sz + sizeof(bigint);
                }
            } else if (img->state == M) {
                sz = sz + sizeof(multiplicity);
                sz = sz + sizeof(void*) * img->mult->capacity;
                for (int i = 0; i < img->mult->amount; i++) {
                    sz = sz + image_get_size_deep(img->mult->members[i]);
                }
            } else {
                sz = sz + sizeof(Domain);
            }
            break;
        }
        case ARRAY: {
            sz = sz + sizeof(struct ArrayImage);
            struct ArrayImage *img = (struct ArrayImage*)image;
            if (img->state == I) {
                sz = sz + (sizeof(void*) * img->i_data.length);
            } else if (img->state == M) {
                sz = sz + sizeof(multiplicity);
                sz = sz + sizeof(void*) * img->mult->capacity;
                for (int i = 0; i < img->mult->amount; i++) {
                    sz = sz + image_get_size_deep(img->mult->members[i]);
                }
            } else {
                sz = sz + sizeof(Domain);
            }
            break;
        }
        case PREDICATE_FACT: {
            sz = sz + sizeof(struct PredicateImage);
            sz = sz + sizeof(struct InstanceSet);
            sz = sz + sizeof(void*);
            break;
        }
        default: break;
    }
    return sz;
}

void instance_lock(Instance *ins) {
    ins->locked = true;
}

void instance_unlock(Instance *ins) {
    ins->locked = false;
}

string instance_repr(Instance *ins) {
    return ins->repr(ins);
}

qresp respond(qresp_state s, string e) {
    qresp qr;
    qr.state = s;
    qr.error = e;
    if (qr.state == q_error) {
        if (LAURE_TRACE && LAURE_TRACE->length > 1)
            laure_trace_print();
    } else {
        laure_trace_linked *l = laure_trace_last_linked();
        if (l != NULL) {
            if (l->prev != NULL)
                l->prev->next = NULL;
            else l = NULL;
        }
    }
    return qr;
}
