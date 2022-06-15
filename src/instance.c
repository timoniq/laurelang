#include "laureimage.h"

struct Translator *INT_TRANSLATOR, 
                  *CHAR_TRANSLATOR, 
                  *STRING_TRANSLATOR,
                  *ARRAY_TRANSLATOR,
                  *ATOM_TRANSLATOR;

string MOCK_NAME;
qresp  MOCK_QRESP;
string DEFAULT_ANONVAR = NULL;
char* IMG_NAMES[] = {"Integer", "Char", "Array", "Atom", "Predicate", "Constraint", "Structure", "", "[External]"};

#define REC_TYPE(rec) gen_resp (*rec)(void*, void*)

int char_isnumber(int _c) {
    return _c >= 48 && _c <= 57;
}

gen_resp form_gen_resp(bool state, qresp qr) {
    gen_resp gr = {state, qr};
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

// These methods should be implemented for each data type:
//   translator
//   eq
//   repr
//   deepcopy
//   free
//   generate

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

        if (raw[0] == '<') {
            left_secl = true;
            raw++;
        }

        if (raw[0] == '-') {
            left_minus = true;
            raw++;
        }

        string left = malloc(strlen(raw) - strlen(_temp));
        strncpy(left, raw, strlen(raw) - strlen(_temp));

        _temp = _temp + 2;

        uint right_n = strlen(_temp);

        if (lastc(raw) == '>') {
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
        } else {
            if (img2_t->u_data->lborder.t != INFINITE)
                int_domain_gt(img1_t->u_data, img2_t->u_data->lborder);
            if (img2_t->u_data->rborder.t != INFINITE)
                int_domain_lt(img1_t->u_data, img2_t->u_data->rborder);
            if (img1_t->u_data->lborder.t != INFINITE)
                int_domain_gt(img2_t->u_data, img1_t->u_data->lborder);
            if (img1_t->u_data->rborder.t != INFINITE)
                int_domain_lt(img2_t->u_data, img1_t->u_data->rborder);
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

struct ArrayImage *laure_create_array_u(Instance *el_t) {
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
        if (array->length_lid) {
            Instance *llen = laure_scope_find_by_link(scope, array->length_lid, true);
            if (llen && ! llen->locked) {
                struct IntImage *limg = laure_create_integer_i((int)length);
                if (! image_equals(llen->image, limg)) {
                    image_free(limg);
                    return false;
                }
            }
        }
        
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
                strcat(buff, "...");
                break;
            }
            strcat(buff, repr);
            len += strlen(buff);
            free(repr);
            if (idx != img->i_data.length - 1) {
                strcat(buff, ", ");
                len += 2;
            }
            idx++;
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
        // printf("both arrays are ambiguative, todo\n");
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

typedef struct array_linker_ctx {
    Instance *linked_instance;
    array_linked_t *tail;
    uint remaining_length;
    struct ArrayImage *general_ary_img;
    laure_scope_t *scope;

    REC_TYPE(final_rec);
    void *final_external_ctx;
} array_linker_ctx;

gen_resp array_tail_linker_generator(
    void *im,
    array_linker_ctx *ctx
) {
    Instance *back_instance = ctx->linked_instance;
    array_linked_t *back_linked = ctx->tail;
    uint back_rl = ctx->remaining_length;

    void *back = ctx->linked_instance->image;
    ctx->linked_instance->image = im;
    gen_resp GR;
    if (! ctx->remaining_length) {
        GR = ctx->final_rec(ctx->general_ary_img, ctx->final_external_ctx);
    } else {
        uint i = 0;
        array_linked_t *linked = ctx->tail;
        bool g = false;
        while (i < ctx->remaining_length && linked) {
            Instance *ins = linked->data;
            if (! instantiated(ins)) {
                ctx->linked_instance = ins;
                ctx->tail = linked->next;
                ctx->remaining_length = ctx->remaining_length - i - 1;
                GR = image_generate(ctx->scope, ins->image, array_tail_linker_generator, ctx);
                g = true;
                break;
            }
            linked = linked->next;
            i++;
        }
        if (! g) {
            GR = ctx->final_rec(ctx->general_ary_img, ctx->final_external_ctx);
        }
    }

    // return to prev state
    back_instance->image = back;
    ctx->linked_instance = back_instance;
    ctx->tail = back_linked;
    ctx->remaining_length = back_rl;
    return GR;
}

typedef struct array_det {
    struct ArrayImage *im;
    laure_scope_t *scope;
    REC_TYPE(final_rec);
    void *final_external_ctx;
} array_det;

gen_resp array_length_receiver(
    bigint *bi_len,
    array_det *ctx
) {
    uint length = (uint)bigint_double(bi_len);

    void *restore_llen, *limg;
    restore_llen = NULL;
    limg = NULL;
    if (ctx->im->length_lid) {
        Instance *llen = laure_scope_find_by_link(ctx->scope, ctx->im->length_lid, false);
        if (llen && ! llen->locked) {
            restore_llen = llen->image;
            limg = laure_create_integer_i((int)length);
            llen->image = limg;
        }
    }
    struct ArrayUData u_data = ctx->im->u_data;
    struct ArrayIData i_data;
    i_data.linked = NULL;
    i_data.length = length;
    uint i = length;

    while (i) {
        array_linked_t *linked = malloc(sizeof(array_linked_t));
        linked->next = i_data.linked;
        linked->data = instance_deepcopy(ctx->scope, MOCK_NAME, ctx->im->arr_el);
        i_data.linked = linked;
        i--;
    }
    ctx->im->state = I;
    ctx->im->i_data = i_data;

    gen_resp GR = image_generate(ctx->scope, ctx->im, ctx->final_rec, ctx->final_external_ctx);
    
    ctx->im->state = U;
    ctx->im->u_data = u_data;

    if (limg) {
        Instance *llen = laure_scope_find_by_link(ctx->scope, ctx->im->length_lid, false);
        if (llen && ! llen->locked) llen->image = restore_llen;
        image_free(limg);
    }
    
    return GR;
}

gen_resp array_generate(
    laure_scope_t *scope, 
    struct ArrayImage *im, 
    REC_TYPE(rec), 
    void *external_ctx
) {
    if (im->state == I) {
        uint i = 0;
        array_linked_t *linked = im->i_data.linked;
        while (i < im->i_data.length && linked) {
            Instance *ins = linked->data;
            if (! instantiated(ins)) {
                array_linker_ctx ctx[1];
                ctx->final_rec = rec;
                ctx->final_external_ctx = external_ctx;
                ctx->linked_instance = ins;
                ctx->remaining_length = im->i_data.length - i - 1;
                ctx->tail = linked->next;
                ctx->general_ary_img = im;
                ctx->scope = scope;
                return image_generate(scope, ins->image, array_tail_linker_generator, ctx);
            }
            linked = linked->next;
            i++;
        }
        return rec(im, external_ctx);
    } else {
        array_det ctx[1];
        ctx->final_rec = rec;
        ctx->final_external_ctx = external_ctx;
        ctx->im = im;
        ctx->scope = scope;
        Domain *len_copy = int_domain_copy(im->u_data.length);
        gen_resp response = int_domain_generate(len_copy, array_length_receiver, ctx);
        int_domain_free(len_copy);
        return response;
    }
}

/*
   Working with chars
*/

struct CharImage *laure_create_char_u() {
    struct CharImage *img = malloc(sizeof(struct CharImage));
    img->t = CHAR;
    img->state = 0;
    img->translator = CHAR_TRANSLATOR;
    return img;
}

struct CharImage *laure_create_char_i(int c) {
    struct CharImage *img = malloc(sizeof(struct CharImage));
    img->t = CHAR;
    img->state = 1;
    img->c = c;
    img->translator = CHAR_TRANSLATOR;
    return img;
}

struct CharImage *laure_create_charset(string charset) {
    struct CharImage *img = malloc(sizeof(struct CharImage));
    img->t = CHAR;
    img->state = 2;
    img->charset = charset;
    img->translator = CHAR_TRANSLATOR;
    return img;
}

int laure_convert_ch_esc(int c) {
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

int laure_convert_esc_ch(int c, char *write) {
    switch (c) {
        case '\a': strcpy(write, "\\a"); return 2;
        case '\b': strcpy(write, "\\b"); return 2;
        case '\e': strcpy(write, "\\e"); return 2;
        case '\f': strcpy(write, "\\f"); return 2;
        case '\n': strcpy(write, "\\n"); return 2;
        case '\r': strcpy(write, "\\r"); return 2;
        case '\t': strcpy(write, "\\t"); return 2;
        case '\v': strcpy(write, "\\v"); return 2;
        default: return laure_string_put_char(write, c);
    }
}

bool char_translator(laure_expression_t *exp, void *img_, laure_scope_t *scope) {
    if (exp->t != let_custom && exp->t != let_singlq) return false;

    struct CharImage *img = (struct CharImage*)img_;
    
    if (exp->t == let_custom) {
        if (! ((exp->s[0] == '\'' && lastc(exp->s) == '\'') || (exp->s[0] == '\"' && lastc(exp->s) == '\"'))) return false;
    } else if (exp->t == let_singlq) {
        if (exp->flag > 0) return false;
    }

    if (strlen(exp->s) - (exp->t == let_custom ? 2 : 0) >= 127)
        return false;
    
    char buff[128];
    uint l = 0;

    uint align_left, align_right;
    if (exp->t == let_custom) {
        align_left = 1;
        align_right = 2;
    } else {
        align_left = 0;
        align_right = 0;
    }

    for (uint idx = 0; idx < laure_string_strlen(exp->s) - align_right; idx++) {
        int c = laure_string_char_at_pos(exp->s + align_left, strlen(exp->s) - align_right, idx);
        if (c == '\\') {
            c = laure_convert_ch_esc(
                laure_string_char_at_pos(exp->s + align_left, strlen(exp->s) - align_right, ++idx)
            );
        }
        l += laure_string_put_char(buff + l, c);
    }

    size_t len = laure_string_strlen(buff);
    size_t bufflen = strlen(buff);

    int ch = laure_string_char_at_pos(buff, bufflen, 0);

    switch (img->state) {
        case 0: {
            if (len == 1) {
                img->state = 1;
                img->c = ch;
            } else {
                img->state = 2;
                img->charset = strdup(buff);
            }
            return true;
        }
        case 1: {
            if (len == 1) {
                return img->c == ch;
            } else {
                int seek = img->c;
                for (size_t idx = 0; idx < len; idx++) {
                    int c = laure_string_char_at_pos(buff, bufflen, idx);
                    if (c == '\\') {
                        int c2 = laure_string_char_at_pos(buff, bufflen, ++idx);
                        c = laure_convert_ch_esc(c2);
                    }
                    if (c == seek) return true;
                }
                return false;
            }
        }
        case 2: {
            size_t chset_unicode_len = laure_string_strlen(img->charset);
            size_t chset_byte_sz = strlen(img->charset);
            if (len == 1) {
                int seek = ch;
                for (size_t idx = 0; idx < chset_unicode_len; idx++) {
                    int c = laure_string_char_at_pos(img->charset, chset_byte_sz, idx);
                    if (c == seek) {
                        img->state = 1;
                        img->c = seek;
                        return true;
                    }
                }
                return false;
            } else {
                if (chset_unicode_len > len) return false;
                for (size_t idx = 0; idx < chset_unicode_len; idx++) {
                    int c = laure_string_char_at_pos(img->charset, chset_byte_sz, idx);
                    bool f = false;
                    for (size_t jdx = 0; jdx < len; jdx++) {
                        if (laure_string_char_at_pos(buff, bufflen, jdx) == c) {
                            f = true;
                            break;
                        }
                    }
                    if (! f) return false;
                }
                return true;
            }
            break;
        }
        default: {
            printf("char_translator for state=%d not implemented\n", img->state);
            return false;
        }
    }
}

string char_repr(Instance *ins) {
    struct CharImage *img = (struct CharImage*)ins->image;
    switch (img->state) {
        case 1: {
            char buff[8];
            strcpy(buff, "'");
            laure_convert_esc_ch(img->c, buff + 1);
            strcat(buff, "'");
            return strdup(buff);
        }
        case 2: {
            size_t sz = strlen(img->charset);
            string buff = malloc((sz * 2) + 3);
            strcpy(buff, "\'");
            uint l = 1;
            for (uint idx = 0; idx < laure_string_strlen(img->charset); idx++) {
                int c = laure_string_char_at_pos(img->charset, sz, idx);
                l += laure_convert_esc_ch(c, buff + l);
            }
            strcat(buff, "\'");
            return buff;
        }
        default: {
            return strdup("(char)");
        }
    }
}

bool char_eq(struct CharImage *img1_t, struct CharImage *img2_t) {
    if (img1_t->state == img2_t->state) {
        if (img1_t->state == 1) {
            return img1_t->c == img2_t->c;
        } else if (img1_t->state == 0) {
            return true;
        } else if (img1_t->state == 2) {
            // (intersection)
            struct CharImage *min = img1_t;
            struct CharImage *max = img2_t;
            if (strlen(max->charset) < strlen(min->charset)) {
                struct CharImage *temp = max;
                max = min;
                min = temp;
            }
            char buff[128];
            uint l = 0;
            for (uint idx = 0; idx < laure_string_strlen(min->charset); idx++) {
                int c1 = laure_string_char_at_pos(min->charset, strlen(min->charset), idx);
                for (uint jdx = 0; jdx < laure_string_strlen(max->charset); jdx++) {
                    int c2 = laure_string_char_at_pos(max->charset, strlen(max->charset), jdx);
                    if (c1 == c2) {
                        if (l >= 122)
                            break;
                        l += laure_convert_esc_ch(c1, buff + l);
                    }
                }
            }
            if (! l) return false;
            free(min->charset);
            free(max->charset);
            min->charset = strdup(buff);
            max->charset = strdup(buff);
            return true;
        }
    } else if (img1_t->state || img2_t->state) {
        if (! img1_t->state) {
            img1_t->state = img2_t->state;
            if (img2_t->state == 1) {
                img1_t->c = img2_t->c;
            } else {
                img1_t->charset = strdup(img2_t->charset);
            }
        } else {
            img2_t->state = img1_t->state;
            if (img1_t->state == 1) {
                img2_t->c = img1_t->c;
            } else {
                img2_t->charset = strdup(img1_t->charset);
            }
        }
        return true;
    } else if (img1_t->state == 2 || img2_t->state == 2) {
        struct CharImage *chset = img1_t->state == 2 ? img1_t : img2_t;
        struct CharImage *single = img1_t->state == 2 ? img2_t : img1_t;
        for (uint idx = 0; idx < laure_string_strlen(chset->charset); idx++) {
            int c = laure_string_char_at_pos(chset->charset, strlen(chset->charset), idx);
            if (c == single->c) {
                chset->c = single->c;
                chset->state = 1;
                return true;
            }
        }
        return false;
    }
    return false;
}

struct CharImage *char_deepcopy(struct CharImage *old_img) {
    struct CharImage *image = malloc(sizeof(struct CharImage));
    image->t = CHAR;
    image->translator = old_img->translator;
    image->state = old_img->state;
    switch (image->state) {
        case 1: {
            image->c = old_img->c;
            break;
        }
        case 2: {
            image->charset = strdup(old_img->charset);
            break;
        }
        default: break;
    }
    return image;
}

#define DEFAULT_CHAR '?'

gen_resp char_generate(
    laure_scope_t *scope, 
    struct CharImage *im, 
    REC_TYPE(rec), 
    void *external_ctx
) {
    switch (im->state) {
        case 0: {
            im->state = I;
            im->c = DEFAULT_CHAR;
        }
        case 1: {
            return rec(im, external_ctx);
        }
        case 2: {
            size_t len = laure_string_strlen(im->charset);
            size_t bytes = strlen(im->charset);
            string charset = im->charset;
            for (size_t idx = 0; idx < len; idx++) {
                int c = laure_string_char_at_pos(charset, bytes, idx);
                im->state = 1;
                im->c = c; 
                gen_resp gr = rec(im, external_ctx);
                if (gr.r == 0) return gr;
            }
            im->state = 2;
            im->charset = charset;
            return form_gen_resp(true, MOCK_QRESP);
        }
    }
}

void char_free(struct CharImage *im) {
    if (im->state == 2) {
        free(im->charset);
    }
    free(im);
}

/*
   Working with strings
*/

string string_repr(Instance *ins) {
    struct ArrayImage *image = (struct ArrayImage*) ins->image;
    if (image->state) {
        // unicode character can take up to 5 bytes
        string buff = malloc((image->i_data.length * 5) + 3);
        uint finlen = 2;
        strcpy(buff, "\"");
        array_linked_t *linked = image->i_data.linked;
        for (uint idx = 0; idx < image->i_data.length && linked; idx++) {
            struct CharImage *ch = linked->data->image;
            char chbuff[6];
            finlen += laure_string_put_char(chbuff, ch->c);
            strcat(buff, chbuff);
            linked = linked->next;
        }
        strcat(buff, "\"");
        string final = malloc(finlen + 1);
        strcpy(final, buff);
        free(buff);
        return final;
    } else {
        return strdup("(string)");
    }
}

bool string_translator(laure_expression_t *exp, void *img_, laure_scope_t *scope) {
    if (exp->t != let_custom) return false;
    struct ArrayImage *strarr = (struct ArrayImage*)img_;
    if (! str_starts(exp->s, "\"") && lastc(exp->s) == '\"') return false;
    int len = (int)laure_string_strlen(exp->s + 1) - 1;
    size_t bufflen = strlen(exp->s + 1) - 1;
    if (strarr->state) {
        if (strarr->i_data.length != len) return false;
        array_linked_t *linked = strarr->i_data.linked;
        for (uint idx = 0; idx < len && linked; idx++) {
            int c = laure_string_char_at_pos(exp->s + 1, bufflen, idx);
            struct CharImage *ch = (struct CharImage*)linked->data->image;
            if (c != ch->c) return false;
            linked = linked->next;
        }
        return true;
    } else {
        bigint bi[1];
        bigint_init(bi);
        bigint_from_int(bi, len);
        bool chk = int_domain_check(strarr->u_data.length, bi);
        bigint_free(bi);
        if (! chk) return false;
        array_linked_t *linked = NULL;
        for (int idx = len - 1; idx >= 0; idx--) {
            int c = laure_string_char_at_pos(exp->s + 1, bufflen, idx);
            struct CharImage *ch = laure_create_char_i(c);
            Instance *chins = instance_new(MOCK_NAME, NULL, ch);
            array_linked_t *l = malloc(sizeof(array_linked_t));
            l->next = linked;
            l->data = chins;
            linked = l;
        }
        struct ArrayIData i_data;
        i_data.length = len;
        i_data.linked = linked;
        strarr->state = I;
        strarr->i_data = i_data;
        return true;
    }
}


/*
   Working with atoms
*/

struct AtomImage *laure_atom_universum_create(multiplicity *mult) {
    struct AtomImage *img = malloc(sizeof(struct AtomImage));
    img->t = ATOM;
    img->single = false;
    img->mult = mult;
    img->translator = ATOM_TRANSLATOR;
    return img;
}

void write_atom_name(string r, char *write_to) {
    string s = r;
    if (s[0] == '@') s++;
    if (s[0] == '"') s++;
    size_t len = strlen(s);
    if (lastc(s) == '"') len--;
    if (len > ATOM_LEN_MAX) len = ATOM_LEN_MAX;
    strncpy(write_to, s, len);
    write_to[len] = 0;
} 

bool atom_translator(laure_expression_t *exp, void *img_, laure_scope_t *scope) {
    struct AtomImage *atom = (struct AtomImage*)img_;

    if (exp->t == let_custom || (exp->t == let_atom && (! exp->flag))) {
        // single name passed
        char name[ATOM_LEN_MAX];
        write_atom_name(exp->s, name);
        if (atom->single) {
            // compare single atom names
            return strcmp(atom->atom, name) == 0;
        } else {
            // 1. check if name in universum
            for (uint i = 0; i < atom->mult->amount; i++) {
                string v = atom->mult->members[i];
                if (strcmp(v, name) == 0) {
                    // 2. assign
                    atom->single = true;
                    atom->atom = v;
                    return true;
                }
            }
            return false;
        }
    } else if (exp->t == let_atom) {
        laure_expression_t *ptr = NULL;
        EXPSET_ITER(exp->ba->set, ptr, {
            char name[ATOM_LEN_MAX];
            write_atom_name(ptr->s, name);
            if (atom->single) {
                // compare names
                if (strcmp(name, atom->atom) != 0) return false;
            } else {
                // check all names in atom universum
                bool found = false;
                for (uint i = 0; i < atom->mult->amount; i++) {
                    string v = atom->mult->members[i];
                    if (strcmp(v, name) == 0) {
                        found = true;
                        break;
                    }
                }
                if (! found) return false;
            }
        });
        return true;
    }
}

bool atom_eq(struct AtomImage *img1_t, struct AtomImage *img2_t) {
    if (img1_t->single && img2_t->single) {
        return strcmp(img1_t->atom, img2_t->atom) == 0;

    } else if (img1_t->single || img2_t->single) {
        string single;
        struct AtomImage *mult_img;
        if (img1_t->single) {
            single = img1_t->atom;
            mult_img = img2_t;
        } else {
            single = img2_t->atom;
            mult_img = img1_t;
        }

        for (uint i = 0; i < mult_img->mult->amount; i++) {
            string name = mult_img->mult->members[i];
            if (str_eq(name, single)) {
                // assign
                mult_img->single = true;
                multiplicity_free(mult_img->mult);
                mult_img->atom = name;
                return true;
            }
        }
        return false;
    } else if (! img1_t->single && ! img2_t->single) {
        // intersect
        multiplicity *mult_new = multiplicity_create();
        for (uint i = 0; i < img1_t->mult->amount; i++) {
            string atom_1 = img1_t->mult->members[i];
            for (uint j = 0; j < img2_t->mult->amount; j++) {
                string atom_2 = img2_t->mult->members[j];
                if (strcmp(atom_1, atom_2) == 0) {
                    multiplicity_insert(mult_new, atom_1);
                    break;
                }
            }
        }
        if (! mult_new->amount) {
            multiplicity_free(mult_new);
            return false;
        }
        multiplicity_free(img1_t->mult);
        multiplicity_free(img2_t->mult);
        img1_t->mult = mult_new;
        img2_t->mult = multiplicity_deepcopy(mult_new);
        return true;
    }
    return false;
}

string single_atom_repr(struct AtomImage *atom) {
    assert(atom->single);
    return atom->atom;
}

string atom_repr(Instance *ins) {
    struct AtomImage *atom = ins->image;
    if (atom->single) {
        string r = single_atom_repr(atom);
        string ns = malloc(strlen(r) + 2);
        ns[0] = '@';
        strcpy(ns + 1, r);
        ns[strlen(r) + 2] = 0;
        return ns;
    } else {
        char buff[512];
        strcpy(buff, "@{");
        uint used = 2;
        for (uint i = 0; i < atom->mult->amount; i++) {
            string n = atom->mult->members[i];
            if (used + strlen(n) + 3 >= 512) break;
            strcat(buff, n);
            used += strlen(n);
            if (i < atom->mult->amount - 1) {
                strcat(buff, ", ");
                used += 2;
            }
        }
        strcat(buff, "}");
        return strdup(buff);
    }
}

struct AtomImage *atom_deepcopy(struct AtomImage *old_img) {
    struct AtomImage *atom = malloc(sizeof(struct AtomImage));
    *atom = *old_img;
    if (! old_img->single) {
        atom->mult = multiplicity_deepcopy(old_img->mult);
    }
    return atom;
}

void atom_free(struct AtomImage *im) {
    if (! im->single) {
        multiplicity_free(im->mult);
    }
    free(im);
}

gen_resp atom_generate(
    laure_scope_t *scope, 
    struct AtomImage *im, 
    REC_TYPE(rec), 
    void *external_ctx
) {
    if (im->single) {
        return rec(im, external_ctx);
    } else {
        multiplicity *temp = im->mult;
        for (uint i = 0; i < temp->amount; i++) {
            string name = temp->members[i];
            im->single = true;
            im->atom = name;
            gen_resp GR = rec(im, external_ctx);
            if (! GR.r) {
                return GR;
            }
        }
        im->single = false;
        im->mult = temp;
        return form_gen_resp(true, respond(q_yield, 1));
    }
}

/*
   Global methods
*/

// Set up translators

void laure_set_translators() {
    INT_TRANSLATOR = new_translator('i', int_translator);
    CHAR_TRANSLATOR = new_translator('c', char_translator);
    ARRAY_TRANSLATOR = new_translator('a', array_translator);
    STRING_TRANSLATOR = new_translator('s', string_translator);
    ATOM_TRANSLATOR = new_translator('@', atom_translator);
    MOCK_NAME = strdup( "$mock_name" );
    MOCK_QRESP.state = q_true;
    MOCK_QRESP.payload = 0;
    DEFAULT_ANONVAR = strdup( "_" );
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
        case CHAR: {
            return char_eq((struct CharImage*) img1, (struct CharImage*) img2);
        }
        case ARRAY: {
            return array_eq((struct ArrayImage*) img1, (struct ArrayImage*) img2);
        }
        case ATOM: {
            return atom_eq((struct AtomImage*) img1, (struct AtomImage*) img2);
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
        case CHAR: {
            return char_generate(scope, (struct CharImage*) img, rec, external_ctx);
        }
        case ARRAY: {
            return array_generate(scope, (struct ArrayImage*) img, rec, external_ctx);
        }
        case ATOM: {
            return atom_generate(scope, (struct AtomImage*) img, rec, external_ctx);
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
    if (! img) return NULL;
    laure_image_head head = read_head(img);
    switch (head.t) {
        case INTEGER: {
            return int_deepcopy((struct IntImage*)img);
        }
        case CHAR: {
            return char_deepcopy((struct CharImage*)img);
        }
        case ARRAY: {
            return array_copy(img);
        }
        case ATOM: {
            return atom_deepcopy((struct AtomImage*) img);
        }
        case CONSTRAINT_FACT:
        case PREDICATE_FACT: {
            return img;
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
        case ARRAY: {
            struct ArrayImage *ary = ((struct ArrayImage*)ins->image);
            if (ary->state != I) return false;
            uint i = 0;
            array_linked_t *linked = ary->i_data.linked;
            while (i < ary->i_data.length && linked) {
                if (! instantiated(linked->data)) return false;
                linked = linked->next;
                i++;
            }
            return true;
        }
        case ATOM:
            return ((struct AtomImage*)ins->image)->single;
        case CONSTRAINT_FACT:
        case PREDICATE_FACT:
            return true;
        case CHAR:
            return ((struct CharImage*)ins->image)->state == I;
        default: return true;
    }
}

// Image free

void image_free(void *image) {
    if (! image) return;
    laure_image_head head = read_head(image);
    switch (head.t) {
        case INTEGER: {
            int_free((struct IntImage*)image);
            break;
        }
        case CHAR: {
            char_free((struct CharImage*)image);
            break;
        }
        case ATOM: {
            atom_free((struct AtomImage*)image);
            break;
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

Instance *instance_shallow_copy(Instance *from_instance) {
    if (! from_instance) return NULL;
    Instance *instance = malloc(sizeof(Instance));
    *instance = *from_instance;
    return instance;
}

qresp respond(qresp_state s, string p) {
    qresp qr;
    qr.state = s;
    qr.payload = p;
    return qr;
};

struct Translator *new_translator(char identificator, bool (*invoke)(string, void*, laure_scope_t*)) {
    struct Translator *tr = malloc(sizeof(struct Translator));
    tr->invoke = invoke;
    tr->identificator = identificator;
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

            if (str_eq(arg->s, "_")) {
                pf->interior.argn[i] = DEFAULT_ANONVAR;
                pos++;
                continue;
            }

            // changing arg names
            char argn[32];

            snprintf(argn, 32, "$%d", i);
            pf->interior.argn[i] = strdup(argn);

            if (arg->t == let_var) {
                // |  cast naming |
                // |   var to var |
                laure_expression_t *old_name_var = laure_expression_create(let_var, "", false, pf->interior.argn[i], 0, NULL, arg->fullstring);
                laure_expression_set *set = laure_expression_set_link(NULL, old_name_var);
                set = laure_expression_set_link(set, arg);
                laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, 2, 0);
                laure_expression_t *name_expression = laure_expression_create(let_name, "", false, NULL, 0, ba, arg->fullstring);
                nset = laure_expression_set_link(nset, name_expression);
            } else {
                // |  cast assert |
                // | var to value |
                laure_expression_t *var = laure_expression_create(let_var, "", false, pf->interior.argn[i], 0, NULL, arg->fullstring);
                laure_expression_set *set = laure_expression_set_link(NULL, var);
                set = laure_expression_set_link(set, arg);
                laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, 2, 0);
                laure_expression_t *assert_expression = laure_expression_create(let_assert, "", false, NULL, 0, ba, arg->fullstring);
                complicated_nset = laure_expression_set_link_branch(complicated_nset, laure_expression_compose_one(assert_expression));
            }

            pos++;
        }

        if (has_resp) {
            pf->interior.respn = strdup("$R");
            laure_expression_t *exp = laure_expression_set_get_by_idx(exp_set, pos);

            if (str_eq(exp->s, "_")) {
                free(pf->interior.respn);
                pf->interior.respn = DEFAULT_ANONVAR;
            }

            else if (exp->t == let_var) {
                // cast naming
                // var to var
                
                laure_expression_t *old_name_var = laure_expression_create(let_var, "", false, pf->interior.respn, 0, NULL, exp->fullstring);
                laure_expression_set *set = laure_expression_set_link(NULL, old_name_var);
                set = laure_expression_set_link(set, exp);
                laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, 2, 0);
                laure_expression_t *name_expression = laure_expression_create(let_name, "", false, NULL, 0, ba, exp->fullstring);
                nset = laure_expression_set_link(nset, name_expression);
                
            } else {
                laure_expression_t *var = laure_expression_create(let_var, "", false, pf->interior.respn, 0, NULL, exp->fullstring);
                laure_expression_set *set = laure_expression_set_link(NULL, var);
                set = laure_expression_set_link(set, exp);
                laure_expression_compact_bodyargs *ba = laure_bodyargs_create(set, 2, 0);
                laure_expression_t *assert_expression = laure_expression_create(let_assert, "", false, NULL, 0, ba, exp->fullstring);
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

/* ============
Data type declaration set
============ */

laure_typeset *laure_typeset_new() {
    laure_typeset *typeset = malloc(sizeof(laure_typeset));
    typeset->data = NULL;
    typeset->length = 0;
    return typeset;
}

void laure_typeset_push_instance(laure_typeset *ts, Instance *instance) {
    ts->length++;
    ts->data = realloc(ts->data, sizeof(laure_typedecl) * ts->length);
    ts->data[ts->length - 1].t = td_instance;
    ts->data[ts->length - 1].instance = instance;
}

void laure_typeset_push_decl(laure_typeset *ts, string generic_name) {
    ts->length++;
    ts->data = realloc(ts->data, sizeof(laure_typedecl) * ts->length);
    ts->data[ts->length - 1].t = td_generic;
    ts->data[ts->length - 1].instance = generic_name;
}

bool laure_typeset_all_instances(laure_typeset *ts) {
    for (uint i = 0; i < ts->length; i++) {
        if (ts->data[i].t != td_instance) return false;
    }
    return true;
}

laure_typedecl *laure_typedecl_instance_create(Instance *instance) {
    laure_typedecl *td = malloc(sizeof(laure_typedecl));
    td->t = td_instance;
    td->instance = instance;
    return td;
}

laure_typedecl *laure_typedecl_generic_create(string generic_name) {
    laure_typedecl *td = malloc(sizeof(laure_typedecl));
    td->t = td_generic;
    td->generic = generic_name;
    return td;
}

/* =================
Working with 
predicate variations 
================= */
struct PredicateImage *predicate_header_new(laure_typeset *args, laure_typedecl *resp, bool is_constraint) {
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

string predicate_repr(Instance *ins) {
    char buff[128];

    char argsbuff[64] = {0};
    char respbuff[64] = {0};

    struct PredicateImage *img = (struct PredicateImage*)ins->image;

    for (int i = 0; i < img->header.args->length; i++) {
        laure_typedecl td = img->header.args->data[i];
        string argn;

        if (td.t == td_instance) {
            argn = img->header.args->data[i].instance->name;
        } else if (td.t == td_generic) {
            argn = img->header.args->data[i].generic;
        }
        if (i != 0) {
            strcat(argsbuff, ", ");
            strcat(argsbuff, argn);
        } else strcpy(argsbuff, argn);
        
        if (td.t == td_generic) {
            uint n = img->header.nestings[i];
            while (n > 0) {
                strcat(argsbuff, "[]");
                n--;
            }
        }
    }

    if (img->header.resp != NULL) {
        string rs;
        if (img->header.resp->t == td_instance) {
            rs = img->header.resp->instance->name;
        } else {
            rs = img->header.resp->generic;
        }
        snprintf(respbuff, 64, " -> %s", rs);
        if (img->header.resp->t == td_generic) {
            uint n = img->header.response_nesting;
            //! todo add overflow protection
            while (n > 0) {
                strcat(respbuff, "[]");
                n--;
            }
        }
    } else
        respbuff[0] = 0;

    string name = ins->name;
    if (img->is_primitive) name = "\0";
    
    snprintf(buff, 128, "(?%s(%s)%s)", name, argsbuff, respbuff);
    return strdup(buff);
}

string constraint_repr(Instance *ins) {
    string r = predicate_repr(ins);
    r[1] = '#';
    return r;
}

/* ==========
Miscellaneous
========== */
void instance_lock(Instance *ins) {
    ins->locked = true;
}

void instance_unlock(Instance *ins) {
    ins->locked = false;
}

string instance_get_doc(Instance *ins) {
    return ins->doc ? (strlen(ins->doc) ? ins->doc : NULL) : NULL;
}

Instance *laure_unwrap_nestings(Instance *wrapped, uint redundant_nestings) {
    uint n = redundant_nestings;
    Instance *unwrapped = wrapped;
    while (n > 0) {
        if (read_head(unwrapped->image).t != ARRAY) return NULL;
        unwrapped = ((struct ArrayImage*)unwrapped->image)->arr_el;
        n--;
    }
    return unwrapped;
}