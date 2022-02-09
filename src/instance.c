#include "laureimage.h"
#include <ctype.h>

#ifndef isnumber
int isnumber(int _c) {
    return _c >= 48 && _c <= 57;
}
#endif

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

    return (void*)img;
}

void *array_u_new(Instance *element) {
    struct ArrayImage *img = malloc(sizeof(struct ArrayImage));

    struct ArrayUData u_data;
    u_data.length = int_domain_new();

    IntValue zero = {INCLUDED, 0};
    int_domain_gt(u_data.length, zero);

    img->t = ARRAY;
    img->arr_el = element;
    img->translator = new_translator(array_translator);
    img->state = U;
    img->u_data = u_data;
    img->length_lid = 0;

    return (void*)img;
}

void *abstract_new(string atom) {
    return NULL;
}

void *predicate_header_new(struct InstanceSet *args, Instance *resp, bool is_constraint) {
    struct PredicateImage *img = malloc(sizeof(struct PredicateImage));

    img->t = !is_constraint ? PREDICATE_FACT : CONSTRAINT_FACT;
    
    struct PredicateHeaderImage header;
    header.args = args;
    header.resp = resp;
    
    img->translator = NULL;
    img->header = header;
    img->variations = pvs_new();
    
    return img;
}

struct ArrayIData array_i_data_deepcopy(laure_stack_t *stack, struct ArrayIData old_i_data);

void *image_deepcopy(laure_stack_t *stack, void *img) {
    assert(img != NULL);
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
                } else if (old_img->datatype == CINT)
                    new_img->i_data_cint = old_img->i_data_cint;
                else if (old_img->datatype == CHAR_T)
                    new_img->i_data_char = old_img->i_data_char;
                else
                    printf("Unknown datatype for int\n");
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
            new_img_ptr = img;
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
            return ehead.copy(img);
        }
        default: {
            printf("not implemented deepcopy %d\n", head.t);
            break;
        };
    };

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
    laure_expression_t *exp,
    int priority
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
    ctx->aid->i_data.array = malloc(sizeof(void*) * old->aid->i_data.length);
    for (int i = 0; i < old->aid->i_data.length; i++) {
        ctx->aid->i_data.array[i] = instance_deepcopy(old->stack, "el", old->aid->i_data.array[i]);
    }
    ctx->aid->arr_el = old->aid->arr_el;
    ctx->aid->i_data.length = old->aid->i_data.length;
    return ctx;
}

string convert_char_array(struct ArrayIData char_arr);

gen_resp generate_array_tail(void *img, void *ctx_) {
    GenArrayCtx *old_ctx = (GenArrayCtx*)ctx_;
    GenArrayCtx *ctx = copy_gen_ary_ctx(old_ctx);

    ctx->aid->i_data.array[ctx->aid->i_data.length] = instance_new("el", "", img);
    ctx->aid->i_data.length++;

    if (ctx->length == ctx->aid->i_data.length) {

        gen_resp gr = ctx->rec(image_deepcopy(ctx->stack, ctx->aid), ctx->external_ctx);
        // ctx->aid->i_data.length--;
        ctx->aid->i_data.length--;
        return gr;
    } else {
        return image_generate(ctx->stack, get_image(ctx->im->arr_el), generate_array_tail, ctx);
    }
}

bool img_equals(void* img1, void* img2);

gen_resp generate_array(bigint *length_, void *ctx_) {
    GenCtx *ctx = (GenCtx*)ctx_;
    int length = (int)bigint_double(length_);
    if (length == 0) {
        return ctx->rec(array_i_new(NULL, 0), ctx->external_ctx);
    }

    struct ArrayImage *arr_im = (struct ArrayImage*)ctx->im;
    if (arr_im->length_lid) {
        Cell cell = laure_stack_get_cell_by_lid(ctx->stack, arr_im->length_lid, true);
        struct IntImage *img = (struct IntImage*) get_image(cell.instance);
        img->state = I;
        img->datatype = BINT;
        img->i_data = length_;
        //! fixme smart allocation
    }

    GenArrayCtx *gen_ary_ctx = malloc(sizeof(GenArrayCtx));
    gen_ary_ctx->aid = array_i_new(malloc(sizeof(void*) * length), length);
    gen_ary_ctx->length = length;
    gen_ary_ctx->im = ctx->im;
    gen_ary_ctx->external_ctx = ctx->external_ctx;
    gen_ary_ctx->rec = ctx->rec;
    gen_ary_ctx->stack = ctx->stack;
    return image_generate(ctx->stack, get_image(gen_ary_ctx->im->arr_el), generate_array_tail, gen_ary_ctx);
}

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
        default: {
            printf("not impl %d\n", head.t);
            break;
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
        case CINT: {
            bigint *bi = malloc(sizeof(bigint));
            bigint_init(bi);
            bigint_from_int(bi, img->i_data_cint);
            img->i_data = bi;
            break;
        }
        case CHAR_T: {
            bigint *bi = malloc(sizeof(bigint));
            bigint_init(bi);
            bigint_from_int(bi, (int)img->i_data_char);
            img->i_data = bi;
            break;
        }
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
    return ins->name;
}

string predicate_repr(Instance *ins) {
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
    
    snprintf(buff, 128, "(?%s(%s)%s)", ins->name, argsbuff, respbuff);
    return buff;
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
    return buff;
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

string array_img_repr(struct ArrayImage *img) {
    printf("todo repr\n");
    return strdup("todo repr");
}

string array_repr(Instance *ins) {

    struct ArrayImage *img = (struct ArrayImage*)ins->image;

    if (img->state == M) {
        char buffer[64];
        strcat(buffer, "{");
        printf("todo array_repr\n");
        strcat(buffer, "}");
        return buffer;
    } else {
        return array_img_repr(img);
    }
}

Instance *instance_new(string name, string doc, void *image) {
    Instance *ins = malloc(sizeof(Instance));
    ins->name = name;
    ins->derived = NULL;
    ins->doc = doc;
    ins->repr = default_repr;
    ins->locked = false;
    ins->image = image;
    return ins;
};

void *instance_free(Instance* instance) {
    if (instance == NULL) return NULL;
    if (instance->image == NULL) return NULL;
    // image_free(instance->image, true);
    //free(instance->name);
    free(instance);
    instance = NULL;
    return instance;
}

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
    /*
    for (int i = 0;;) {
        bool last = false;

        // reading next expression
        string el_mstring = read_until_c(',', mstring, &i);

        // if expression is empty - finish
        // else if no delimiter for the last expression - read it
        if (el_mstring == NULL || strlen(el_mstring) == 0) {
            el_mstring = read_to_end(mstring, i);
            last = true;
            if (strlen(el_mstring) == 0 || el_mstring == NULL) {
                break;
            }
        }

        // clean expression and make a step from delimiter
        string el_s = clean_s(el_mstring);
        i++;

        void *el_img = image_deepcopy(stack, rimg);
        bool success = transl(el_s, el_img, stack, true);
        if (!success) {
            multiplicity_free(mult);
            image_free(el_img, true);
            return NULL;
        }
        multiplicity_insert(mult, el_img);

        if (last) {
            break;
        }
    }
    */
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
            if (!isnumber(left[i])) {
                free(left); free(right);
                return false;
            };
        
        for (int i = 0; i < strlen(right); i++) 
            if (!isnumber(right[i])) {
                free(left); free(right);
                return false;
            };
        
        bigint *bi_left = malloc(sizeof(bigint));
        bigint_init(bi_left);
        bigint_from_str(bi_left, left);
        if (left_minus) bigint_negate(bi_left);
        IntValue left_value = {left_secl ? SECLUDED : INCLUDED, bi_left};

        bigint *bi_right = malloc(sizeof(bigint));
        bigint_init(bi_right);
        bigint_from_str(bi_right, right);
        if (right_minus) bigint_negate(bi_right);
        IntValue right_value = {right_secl ? SECLUDED : INCLUDED, bi_right};

        Domain *new_domain = int_domain_new();
        int_domain_gt(new_domain, left_value);
        int_domain_lt(new_domain, right_value);

        img->t = U;
        img->u_data = new_domain;
        return true;
    }

    bool minus = false;
    if (raw[0] == '-') {minus = true; raw++;}
    else if (raw[0] == '+') raw++;

    for (int i = 0; i < strlen(raw); i++) 
        if (!isnumber(raw[i])) return false;
    
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
    printf("translate char\n");
    return true;
}

bool array_translator(laure_expression_t *exp, void* rimg, laure_stack_t *stack) {
    printf("translate array\n");
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
    printf("translate string\n");
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
            if (((struct ArrayImage*)img1)->state != I || ((struct ArrayImage*)img2)->state != I) return true;
            if (((struct ArrayImage*)img1)->i_data.length != ((struct ArrayImage*)img2)->i_data.length) return false;

            for (int i = 0; i < ((struct ArrayImage*)img1)->i_data.length; i++) {
                if (!img_equals(((struct ArrayImage*)img1)->i_data.array[i]->image, ((struct ArrayImage*)img2)->i_data.array[i]->image)) return false;
            }

            return true;
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

control_ctx *create_control_ctx(laure_stack_t *stack, qcontext* qctx, var_process_kit* vpk, void* data) {
    control_ctx *ptr = malloc(sizeof(control_ctx));
    ptr->stack = stack;
    ptr->data = data;
    ptr->qctx = qctx;
    ptr->vpk = vpk;
    return ptr;
}

/*
void laure_gc_image_mark(laure_gc_treep_t *GC, void *img_) {
    if (!img_) return;

    struct ImageHead head = read_head(img_);

    switch (head.t) {
        case INTEGER: {
            struct IntImage *img = (struct IntImage*)img_;
            if (img->state == I) {
            } else if (img->state == U) {
            } else {
            }
        }
    }
}
*/

// returns false if image is not deepcopied
bool image_free(void *img_raw, bool free_ptr) {
    if (img_raw == NULL) return false;
    struct ImageHead head = read_head(img_raw);

    switch (head.t) {
        case INTEGER: {
            struct IntImage *img = (struct IntImage*)img_raw;
            if (img->state == I) {
                if (img->datatype == BINT) bigint_free(img->i_data);
            } else if (img->state == M) {
                multiplicity_free(img->mult);
            } else {
                int_domain_free(img->u_data);
                img->u_data = NULL;
            }
            if (free_ptr)
                free(img_raw);
            break;
        }
        case ARRAY: {
            struct ArrayImage *img = (struct ArrayImage*)img_raw;
            free(img->arr_el);
            if (img->state == I) {
                printf("freeing array\n");
                for (int i = 0; i < img->i_data.length; i++) {
                    instance_free(img->i_data.array[i]);
                }
                free(img->i_data.array);
            } else if (img->state == M) {
                multiplicity_free(img->mult);
            } else {
                int_domain_free(img->u_data.length);
                img->u_data.length = NULL;
            }
            if (free_ptr)
                free(img_raw);
            break;
        }
        default: return false;
    }
    return true;
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
        i_data.array[i] = instance_deepcopy(stack, old_i_data.array[i]->name, old_i_data.array[i]);
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
                } else if (img->datatype == CINT) {
                    sz = sz + sizeof(int);
                } else if (img->datatype == CHAR_T) {
                    sz = sz + sizeof(char);
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


string instance_repr(Instance *ins) {
    return ins->repr(ins);
}

qresp respond(qresp_state s, string e) {
    qresp qr;
    qr.state = s;
    qr.error = e;
    if (qr.state == q_error) {
        if (LAURE_TRACE->length > 1)
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
