#include "laureimage.h"

struct Translator *INT_TRANSLATOR, 
                  *CHAR_TRANSLATOR, 
                  *STRING_TRANSLATOR,
                  *ARRAY_TRANSLATOR,
                  *ATOM_TRANSLATOR;

string MOCK_NAME;
char* IMG_NAMES[] = {"Integer", "Char", "Array", "Atom", "Predicate", "Constraint", "Structure", "", "[External]"};

#define REC_TYPE(rec) gen_resp (*rec)(void*, void*)

int char_isnumber(int _c) {
    return _c >= 48 && _c <= 57;
}

/*
   Multiplicities
*/

multiplicity *multiplicity_create() {
    multiplicity *mult = malloc(sizeof(multiplicity));
    mult->amount = 0;
    mult->capacity = MULTIPLICITY_CAPACITY;
    mult->members = malloc(sizeof(void*) * mult->capacity);
    memset(mult->members, 0, sizeof(void*) * mult->capacity);
    return mult;
}

multiplicity *multiplicity_deepcopy(multiplicity *mult) {
    if (mult == NULL) return NULL;
    multiplicity *new = malloc(sizeof(multiplicity));
    new->amount = mult->amount;
    new->capacity = mult->amount;
    new->members = malloc(sizeof(void*) * mult->amount);
    for (int i = 0; i < mult->amount; i++)
        new->members[i] = mult->members[i];
    return new;
}

void multiplicity_insert(multiplicity *mult, void *ptr) {
    if (mult->amount + 1 > mult->capacity) {
        mult->capacity = mult->capacity * 2;
        mult->members = realloc(mult->members, sizeof(void*) * mult->capacity);
    }
    mult->members[mult->amount] = ptr;
    mult->amount++;
}

void multiplicity_free(multiplicity *mult) {
    free(mult->members);
    free(mult);
}

/* Read image head */

laure_image_head read_head(void *img) {
    assert(img);
    struct ImageHead head;
    memcpy(&head, img, sizeof(struct ImageHead));
    return head;
}


// Enhanced image head reader;
// for external images with special
// copy, eq, free methods.
laure_image_head_enh read_enhanced_head(void *img) {
    struct EnhancedImageHead head;
    memcpy(&head, img, sizeof(struct EnhancedImageHead));
    return head;
}

/*
   Working with integers
*/

// creates image of definite instantiated integer.
struct IntImage *laure_create_integer_i(int value) {
    struct IntImage *image = malloc(sizeof(struct IntImage));
    image->t = INTEGER;
    image->state = I;
    image->i_data = bigint_create(value);
    image->translator = INT_TRANSLATOR;
    return image;
}

// creates image of indefinite integer.
struct IntImage *laure_create_integer_u(Domain *dom) {
    struct IntImage *image = malloc(sizeof(struct IntImage));
    image->t = INTEGER;
    image->state = U;
    image->u_data = int_domain_new();
    image->translator = INT_TRANSLATOR;
    return image;
}

bool int_translator(laure_expression_t *exp, void *img_, laure_scope_t *scope) {
    if (exp->t != let_custom) return false;
    string raw = exp->s;
    struct IntImage *img = (struct IntImage*)img_;

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

string int_repr(Instance *ins) {
    struct IntImage *im = (struct IntImage*)ins->image;
    if (im->state == I) {
        char buff[512];
        bigint_write(buff, 512, im->i_data);
        return strdup(buff);
    } else if (im->state == U) {
        return int_domain_repr(im->u_data);
    }
}

bool int_check(void *img_, bigint *bi) {
    struct IntImage *img = (struct IntImage*)img_;
    if (img->state == I) {
        return bigint_cmp(img->i_data, bi) == 0;
    } else {
        return int_domain_check(img->u_data, bi);
    }
}

bool int_eq(struct IntImage *img1_t, struct IntImage *img2_t) {
    if (img1_t->state == I && img2_t->state == I) {
        return bigint_cmp(img1_t->i_data, img2_t->i_data) == 0;

    } else {
        if (img1_t->state == I) {
            if (! int_domain_check(img2_t->u_data, img1_t->i_data))
                return false;
            
            img2_t->i_data = bigint_copy(img1_t->i_data);
            img2_t->state = I;
        } else if (img2_t->state == I) {
            if (! int_domain_check(img1_t->u_data, img2_t->i_data))
                return false;

            img1_t->i_data = bigint_copy(img2_t->i_data);
            img1_t->state = I;
        }
        return true;
    }
}

struct IntImage *int_deepcopy(struct IntImage *old_img) {
    struct IntImage *new_img = malloc(sizeof(struct IntImage));

    new_img->t = INTEGER;
    new_img->state = old_img->state;

    if (old_img->state == I) new_img->i_data = bigint_copy(old_img->i_data);
    else new_img->u_data = int_domain_copy(old_img->u_data);

    new_img->translator = old_img->translator;
    return new_img;
}

gen_resp int_generator(bigint* i, void *ctx_) {
    GenCtx *ctx = (GenCtx*)ctx_;
    struct IntImage *img = malloc(sizeof(struct IntImage));
    img->t = INTEGER;
    img->i_data = i;
    img->state = I;
    img->translator = INT_TRANSLATOR;
    gen_resp gr = (ctx->rec)(img, ctx->external_ctx);
    free(img);
    return gr;
}

gen_resp int_generate(laure_scope_t *scope, struct IntImage *im, REC_TYPE(rec), void *external_ctx) {
    if (im->state == I) {
        // integer has only one value
        return rec(im, external_ctx);
    } else {
        // choicepoint
        GenCtx *gctx = malloc(sizeof(GenCtx));
        gctx->scope = scope;
        gctx->external_ctx = external_ctx;
        gctx->rec = rec;
        gen_resp gr =  int_domain_generate(im->u_data, int_generator, gctx);
        free(gctx);
        return gr;
    }
}

void int_free(struct IntImage *im) {
    if (im->state == I) {
        bigint_free(im->i_data);
        free(im->i_data);
    } else if (im->state == U) {
        int_domain_free(im->u_data);
    }
    free(im);
}

/*
   Working with arrays
*/

struct ArrayImage *laure_create_array_i(Instance *el_t) {
    struct ArrayImage *img = malloc(sizeof(struct ArrayImage));
    struct ArrayUData u_data;
    u_data.length = int_domain_new();
    IntValue zero = {INCLUDED, bigint_create(0)};
    int_domain_gt(u_data.length, zero);
    img->t = ARRAY;
    img->arr_el = el_t;
    img->translator = ARRAY_TRANSLATOR;
    img->state = U;
    img->u_data = u_data;
    img->length_lid = 0;
    img->ref_count = 0;
    img->ref = NULL;
    return img;
}

bool array_translator(laure_expression_t *exp, void *img_, laure_scope_t *scope) {
    struct ArrayImage *array = (struct ArrayImage*)img_;
    if (exp->t != let_array) return false;

    if (array->state == U) {
        uint length = laure_expression_get_count(exp->ba->set);
        
        array_linked_t *first = NULL;
        array_linked_t *linked = NULL;

        for (int i = 0; i < length; i++) {
            laure_expression_t *el_exp = laure_expression_set_get_by_idx(exp->ba->set, i);

            void *img;
            if (el_exp->t == let_var) {
                Instance *el = laure_scope_find_by_key(scope, el_exp->s, true);
                if (! el) {
                    return false;
                }
                img = el->image;
            } else {
                img = image_deepcopy(scope, array->arr_el->image);

                bool result = read_head(array->arr_el->image).translator->invoke(el_exp, img, scope);
                if (! result) {
                    return false;
                }
            }

            Instance *ins = instance_new(MOCK_NAME, NULL, img);
            ins->repr = array->arr_el->repr;

            if (i == 0) {
                first = malloc(sizeof(array_linked_t));
                first->data = ins;
                first->next = NULL;
                linked = first;
            } else {
                array_linked_t *new = malloc(sizeof(array_linked_t));
                new->data = ins;
                new->next = NULL;
                linked->next = new;
                linked = new;
            }
        }
        array->i_data.length = length;
        array->i_data.linked = first;
        array->state = I;
    } else {
        if (array->i_data.length != exp->ba->body_len) return false;
        uint idx = 0;
        array_linked_t *linked = array->i_data.linked;
        while (idx < array->i_data.length && linked) {
            Instance *el = linked->data;
            bool res = read_head(el->image).translator->invoke(
                laure_expression_set_get_by_idx(exp->ba->set, idx),
                el->image,
                scope
            );
            if (! res) return false;
            linked = linked->next;
            idx++;
        }
    }
    return true;
}

string array_repr(Instance *ins) {
    struct ArrayImage *img = (struct ArrayImage*)ins->image;
    char buff[512];
    uint len = 0;
    if (img->state == I) {
        strcpy(buff, "[");
        uint idx = 0;
        array_linked_t *linked = img->i_data.linked;
        while (idx < img->i_data.length && linked) {
            Instance *el = linked->data;
            string repr = el->repr(el);
            if (len + strlen(repr) >= 500) {
                free(repr);
                strcat(buff, ", ...");
                break;
            }
            strcat(buff, repr);
            len += strlen(buff);
            free(repr);
            if (idx != img->i_data.length - 1) {
                strcat(buff, ", ");
                len += 2;
            }
            linked = linked->next;
        }
        strcat(buff, "]");
    } else if (img->state == U) {
        string arr_el_repr = img->arr_el->repr(img->arr_el);
        string length_repr = int_domain_repr(img->u_data.length);
        snprintf(buff, 512, "[%s x %s]", arr_el_repr, length_repr);
        free(arr_el_repr);
        free(length_repr);
    }
    return strdup( buff );
}

bool array_eq(struct ArrayImage *img1_t, struct ArrayImage *img2_t) {
    if (img1_t->state == I && img2_t->state == I) {
        if (img1_t->i_data.length != img2_t->i_data.length) return false;

        uint idx = 0;
        array_linked_t *linked1 = img1_t->i_data.linked;
        array_linked_t *linked2 = img2_t->i_data.linked;
        while (linked1 && linked2 && idx < img1_t->i_data.length) {
            if (! image_equals(linked1->data->image, linked2->data->image))
                return false;
            linked1 = linked1->next;
            linked2 = linked2->next;
            idx++;
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
        bool chk = int_domain_check(to->u_data.length, bi_len);
        bigint_free(bi_len);

        if (! chk)
            return false;

        int_domain_free(to->u_data.length);
        to->i_data.length = from->i_data.length;
        to->i_data.linked = from->i_data.linked;
        to->state = I;
        return true;
    } else {
        printf("both arrays are ambiguative, todo\n");
        return true;
    }
}

struct ArrayImage *array_copy(struct ArrayImage *old_img) {
    struct ArrayImage *image = malloc(sizeof(struct ArrayImage));
    *image = *old_img;
    if (old_img->state == I) {
        image->i_data = old_img->i_data;
    } else if (image->state == U) {
        image->u_data.length = int_domain_copy(old_img->u_data.length);
    }
    return image;
}

/*
   Global methods
*/

// Set up translators

void laure_set_translators() {
    INT_TRANSLATOR = new_translator(int_translator);
    // CHAR_TRANSLATOR = new_translator(char_translator);
    ARRAY_TRANSLATOR = new_translator(array_translator);
    // STRING_TRANSLATOR = new_translator(string_translator);
    // ATOM_TRANSLATOR = new_translator(atom_translator);
    MOCK_NAME = strdup( "$mock_name" );
}

// Image eq

bool image_equals(void* img1, void* img2) {
    laure_image_head head1 = read_head(img1);
    laure_image_head head2 = read_head(img2);
    
    if (head1.t != head2.t) return false;

    switch (head1.t) {
        case INTEGER: {
            return int_eq((struct IntImage*) img1, (struct IntImage*) img2);
        }
        default:
            return false;
    }
}

// Image generate

gen_resp image_generate(laure_scope_t *scope, void *img, REC_TYPE(rec), void *external_ctx) {
    assert(img != NULL);
    laure_image_head head = read_head(img);
    img = image_deepcopy(scope, img);

    switch (head.t) {
        case INTEGER: {
            return int_generate(scope, (struct IntImage*) img, rec, external_ctx);
        }
        default: {
            char error[128];
            snprintf(error, 128, "no instantiation implemented for %s", IMG_NAMES[head.t]);
            gen_resp gr = {0, respond(q_error, strdup( error ))};
            return gr;
        }
    }
}

// Image deepcopy

void *image_deepcopy(laure_scope_t *stack, void *img) {
    laure_image_head head = read_head(img);
    switch (head.t) {
        case INTEGER: {
            return int_deepcopy(img);
        }
        default:
            break;
    }
    return NULL;
}

bool instantiated(Instance *ins) {
    struct ImageHead head = read_head(ins->image);
    switch (head.t) {
        case INTEGER:
            return ((struct IntImage*)ins->image)->state == I;
        case ARRAY:
            return ((struct ArrayImage*)ins->image)->state == I;
        case ATOM:
            return ((struct AtomImage*)ins->image)->single;
        case CONSTRAINT_FACT:
        case PREDICATE_FACT:
            return true;
        case CHAR:
            return ((struct CharImage*)ins->image)->is_set;
        default: return true;
    }
}

// Image free

void image_free(void *image) {
    laure_image_head head = read_head(image);
    switch (head.t) {
        case INTEGER: {
            int_free(image);
        }
        default: break;
    }
}

Instance *instance_deepcopy(laure_scope_t *scope, string name, Instance *from_instance) {
    if (from_instance == NULL) return from_instance;
    Instance *instance = malloc(sizeof(Instance));
    instance->doc = from_instance->doc;
    instance->image = image_deepcopy(scope, from_instance->image);
    instance->locked = false;
    instance->name = name;
    instance->repr = from_instance->repr;
    return instance;
};

qresp respond(qresp_state s, string e) {
    qresp qr;
    qr.state = s;
    qr.error = e;
    return qr;
};

struct Translator *new_translator(bool (*invoke)(string, void*, laure_scope_t*)) {
    struct Translator *tr = malloc(sizeof(struct Translator));
    tr->invoke = invoke;
    return tr;
};

string default_repr(Instance *ins) {
    return strdup( ins->name );
};

Instance *instance_new(string name, string doc, void *image) {
    Instance *ins = malloc(sizeof(Instance));
    ins->name = name;
    ins->doc = doc;
    ins->repr = default_repr;
    ins->locked = false;
    ins->image = image;
    return ins;
};

struct PredicateCImageHint *hint_new(string name, Instance* instance) {
    struct PredicateCImageHint *hint = malloc(sizeof(struct PredicateCImageHint));
    hint->name = name;
    hint->hint = instance;
    return hint;
}

/*
   Predicate variation set
*/

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

/* 
   Preddata.
   ---
   Args container for cpreds.
   Methods to work with preddata.
*/

preddata *preddata_new(laure_scope_t *scope) {
    preddata *pd = malloc(sizeof(preddata));
    pd->argc = 0;
    pd->resp = NULL;
    pd->argv = malloc(sizeof(struct predicate_arg));
    pd->scope = scope;
    return pd;
};

void preddata_push(preddata *pd, struct predicate_arg pa) {
    pd->argc++;
    pd->argv = realloc(pd->argv, sizeof(struct predicate_arg) * pd->argc);
    struct predicate_arg *pas = pd->argv;
    pas[pd->argc-1] = pa;
};

void preddata_free(preddata *pd) {
    free(pd->argv);
    free(pd);
};

/* =----=
predfinal
=----= */

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

/* =============
  Instance set
(stored on heap)
============= */
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

/* =================
Working with 
predicate variations 
================= */
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

//! regroup later

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

void laure_add_grabbed_link(control_ctx *cctx, ulong nlink) {
    laure_grab_linked *grab = malloc(sizeof(laure_grab_linked));
    grab->link = nlink;
    grab->next = NULL;
    if (! cctx->grabbed) {
        cctx->grabbed = grab;
    } else {
        grab->next = cctx->grabbed;
        cctx->grabbed = grab;
    }
}

void laure_free_grab(laure_grab_linked *grab) {
    if (! grab) return;
    if (grab->next)
        laure_free_grab(grab->next);
    free(grab);
}