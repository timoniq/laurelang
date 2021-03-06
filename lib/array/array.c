#include <laurelang.h>
#include <predpub.h>
#include <laureimage.h>
#include <math.h>
#include <builtin.h>

#define DOC_each "Array enumerating predicate\n```each(array) = element```\nFrom @/array pkg"
#define DOC_by_idx "Predicate of idx to element relation in array\n```array by_idx idx = element```\nFrom @/array pkg"
#define DOC_length "Array length declaration\nFrom @/array pkg"

#define TOO_AMBIG respond(q_error, strdup( "too much ambiguations" ) );

#define LENGTH(image) ((struct ArrayImage*)image)->i_data.length

#define LINKED(image) ((struct ArrayImage*)image)->i_data.linked

#define STATE(image) ((struct ArrayImage*)image)->state

qresp array_predicate_each(preddata *pd, control_ctx *cctx) {
    Instance *arr_ins = pd_get_arg(pd, 0);
    Instance *el_ins = pd->resp;

    if (!arr_ins->image && !el_ins->image)
        return respond(q_error, "each: operating array or element must be specified; add hint");
    
    if (!arr_ins->image) {
        arr_ins->image = laure_create_array_u(el_ins);
        arr_ins->repr = array_repr;
    } else if (!el_ins->image) {
        el_ins->image = image_deepcopy(cctx->scope, ((struct ArrayImage*)arr_ins->image)->arr_el->image);
        el_ins->repr = ((struct ArrayImage*)arr_ins->image)->arr_el->repr;
    }

    struct ArrayImage *arr_img = arr_ins->image;

    if (instantiated(arr_ins)) {

        if (instantiated(el_ins)) {
            uint i = 0;
            array_linked_t *linked = arr_img->i_data.linked;

            while (i < arr_img->i_data.length && linked) {
                Instance *el = linked->data;
                MUST_BE(image_equals(el_ins->image, el->image));
                linked = linked->next;
                i++;
            }
            return respond(q_true, NULL);

        } else {

            Instance *el_ins_copy = instance_deepcopy(cctx->scope, el_ins->name, el_ins);
            bool found = false;

            uint i = 0;
            array_linked_t *linked = arr_img->i_data.linked;

            while (i < arr_img->i_data.length && linked) {
                void *img_copy = image_deepcopy(cctx->scope, el_ins_copy->image);
                Instance *el = linked->data;

                if (image_equals(img_copy, el->image)) {

                    el_ins->image = img_copy;

                    laure_scope_t *nscope = laure_scope_create_copy(cctx, pd->scope);
                    
                    qcontext temp[1];
                    *temp = qcontext_temp(cctx->qctx->next, NULL, cctx->qctx->bag);

                    laure_scope_t *old_sc = pd->scope;
                    qcontext *old_qc = cctx->qctx;
                    cctx->qctx = temp;
                    cctx->scope = nscope;
                    
                    qresp result = laure_start(cctx, cctx->qctx ? cctx->qctx->expset : NULL);
                    if (result.state == q_true || (result.state == q_yield && result.payload == (void*)1)) found = true;

                    cctx->scope = old_sc;
                    cctx->qctx = old_qc;
                    laure_scope_free(nscope);
                }
                linked = linked->next;
                i++;
            }
            return respond(q_yield, (void*)found);
        }
    } else if (instantiated(el_ins)) {
        // every array element is el_ins
        if (! arr_img->arr_el->locked) {
            bool result = image_equals(arr_img->arr_el->image, el_ins->image);
            return respond(result ? q_true : q_false, 0);
        } else {
            void *ar_el_img_copy = image_deepcopy(cctx->scope, arr_img->arr_el->image);
            bool result = image_equals(ar_el_img_copy, el_ins->image);
            MUST_BE_F(result, {image_free(ar_el_img_copy);});
            Instance *arr_el_new = instance_new(MOCK_NAME, SINGLE_DOCMARK, ar_el_img_copy);
            arr_el_new->repr = arr_img->arr_el->repr;
            arr_img->arr_el = arr_el_new;
            return respond(q_true, 0);
        }
    } else {
        arr_img->arr_el->image = el_ins->image;
        return respond(q_true, 0);
    }
    return respond(q_true, 0);
}


struct ArrayIdxEGenCtx {
    control_ctx *cctx;
    uint idx; 
};


qresp array_predicate_by_idx(preddata *pd, control_ctx *cctx) {
    Instance *arr_ins = pd_get_arg(pd, 0);
    Instance *idx_ins = pd_get_arg(pd, 1);
    Instance *el_ins  = pd->resp;

    if (!arr_ins->image && !el_ins->image)
        return respond(q_error, "by_idx: operating array or element must be specified; add hint");
    
    if (!arr_ins->image) {
        arr_ins->image = laure_create_array_u(el_ins);
        arr_ins->repr = array_repr;
    } else if (!el_ins->image) {
        el_ins->image = image_deepcopy(cctx->scope, ((struct ArrayImage*)arr_ins->image)->arr_el->image);
        el_ins->repr = ((struct ArrayImage*)arr_ins->image)->arr_el->repr;
    }

    struct IntImage *idx_img = (struct IntImage*)idx_ins->image;
    struct ArrayImage *arr_img = (struct ArrayImage*)arr_ins->image;
    void *el_img = el_ins->image;

    if (arr_img->state == I) {
        uint len = arr_img->i_data.length;
        if (instantiated(idx_ins)) {

            uint idx = (uint)bigint_double(idx_img->i_data);
            if (idx < 0 || idx >= len) return RESPOND_FALSE;
            uint i = idx;
            array_linked_t *linked = arr_img->i_data.linked;
            while (i && linked) {
                linked = linked->next;
                i--;
            }
            if (!linked || i)
                return RESPOND_FALSE;
            
            Instance *ins = linked->data;
            bool result = image_equals(el_ins->image, ins->image);
            return respond(result ? q_true : q_false, 0);

        } else if (instantiated(el_ins)) {

            bool found = false;
            void *idx_with_dom = image_deepcopy(cctx->scope, idx_img);
            uint idx = 0;
            array_linked_t *linked = arr_img->i_data.linked;

            while (idx < len && linked) {
                bigint *idx_bi = bigint_create(idx);
                bool idx_check = int_check(idx_with_dom, idx_bi);
                if (! idx_check) {
                    bigint_free(idx_bi); 
                    continue;
                }

                Instance *ins = linked->data;

                if (image_equals(ins->image, el_ins->image)) {

                    idx_img->state = I;
                    idx_img->i_data = idx_bi;
                    found = true;

                    laure_scope_t *nscope = laure_scope_create_copy(cctx, pd->scope);

                    qcontext temp[1];
                    *temp = qcontext_temp(cctx->qctx->next, NULL, cctx->qctx->bag);

                    laure_scope_t *old_sc = pd->scope;
                    qcontext *old_qc = cctx->qctx;

                    cctx->qctx = temp;
                    cctx->scope = nscope;
                    
                    qresp result = laure_start(cctx, NULL);
                    if (result.state == q_true || (result.state == q_yield && result.payload == (void*)1)) found = true;
                    else if (result.state == q_stop || result.state == q_error) {
                        laure_scope_free(nscope);
                        bigint_free(idx_bi);
                        if (result.state == q_error) {
                            image_free(idx_with_dom);
                            return result;
                        }
                        break;
                    }

                    cctx->scope = old_sc;
                    cctx->qctx = old_qc;
                    laure_scope_free(nscope);

                } else {
                    bigint_free(idx_bi);
                }

                linked = linked->next;
                idx++;
            }
            image_free(idx_with_dom);
            return respond(q_yield, (void*)found);

        } else {

            bool found = false;
            void *idx_img_copy = image_deepcopy(cctx->scope, idx_img);
            void *el_img_u = image_deepcopy(cctx->scope, el_img);

            uint idx = 0;
            array_linked_t *linked = arr_img->i_data.linked;

            while (idx < len && linked) {
                bigint *idx_bi = bigint_create(idx);
                bool idx_check = int_check(idx_img_copy, idx_bi);
                if (! idx_check) {
                    bigint_free(idx_bi); 
                    continue;
                }

                Instance *ins = linked->data;
                void *el_img_copy = image_deepcopy(cctx->scope, el_img_u);
                bool check = image_equals(el_img_copy, ins->image);
                if (! check) {
                    bigint_free(idx_bi);
                    continue;
                }

                el_ins->image = el_img_copy;
                idx_img->state = I;
                idx_img->i_data = idx_bi;

                laure_scope_t *nscope = laure_scope_create_copy(cctx, pd->scope);
                
                qcontext temp[1];
                *temp = qcontext_temp(cctx->qctx->next, NULL, cctx->qctx->bag);

                laure_scope_t *old_sc = pd->scope;
                qcontext *old_qc = cctx->qctx;
                cctx->qctx = temp;
                cctx->scope = nscope;
                
                qresp result = laure_start(cctx, cctx->qctx ? cctx->qctx->expset : NULL);
                if (result.state == q_true || (result.state == q_yield && result.payload == (void*)1)) found = true;

                cctx->scope = old_sc;
                cctx->qctx = old_qc;
                laure_scope_free(nscope);

                idx++;
                linked = linked->next;
            }

            image_free(idx_img_copy);
            image_free(el_img_u);

            return respond(q_yield, (void*)found);
        }
    } else if (instantiated(idx_ins)) {
        // length > idx
        // if (! img_equals(el_img, arr_img->arr_el->image)) return RESPOND_FALSE;
        IntValue left; left.t = SECLUDED; left.data = bigint_copy(idx_img->i_data);
        int_domain_gt(arr_img->u_data.length, left);
        ref_element ref;
        ref.idx = (uint)bigint_double(idx_img->i_data);
        ref.link_id = pd->resp_link;
        if (arr_img->ref_count) {
            arr_img->ref = laure_realloc(arr_img->ref, sizeof(ref_element) * (arr_img->ref_count + 1));
            arr_img->ref[arr_img->ref_count++] = ref;
        } else {
            arr_img->ref = laure_alloc(sizeof(ref_element));
            arr_img->ref[0] = ref;
            arr_img->ref_count++;
        }
        return RESPOND_TRUE;
    } else if (instantiated(el_ins)) {
        return TOO_AMBIG;
    } else {
        return TOO_AMBIG;
    }
}


qresp array_predicate_length(preddata *pd, control_ctx *cctx) {
    Instance *arr_ins = pd_get_arg(pd, 0);
    Instance *len_ins = pd->resp;

    struct ArrayImage *arr_img = (struct ArrayImage*)arr_ins->image;
    struct IntImage *len_img = (struct IntImage*)len_ins->image;

    if (instantiated(arr_ins)) {
        uint real_len = arr_img->i_data.length;
        void *real_len_img = laure_create_integer_i(real_len);
        if (arr_img->length_lid) {
            Instance *llen = laure_scope_find_by_link(cctx->scope, arr_img->length_lid, true);
            if (llen && ! llen->locked) {
                if (! image_equals(llen->image, real_len_img)) {
                    image_free(real_len_img);
                    return respond(q_false, 0);
                }
            }
        }
        bool result = image_equals(len_img, real_len_img);
        return respond(result ? q_true : q_false, 0);
    } else if (instantiated(len_ins)) {
        if (arr_img->length_lid) {
            Instance *llen = laure_scope_find_by_link(cctx->scope, arr_img->length_lid, true);
            if (llen && ! llen->locked) {
                if (! image_equals(llen->image, len_img)) {
                    return respond(q_false, 0);
                }
            }
        }
        arr_img->u_data.length->t = SINGLE;
        arr_img->u_data.length->lborder.data = len_img->i_data;
        return respond(q_true, 0);
    } else {
        ulong link[1];
        laure_scope_find_by_key_l(pd->scope, len_ins->name, link, true);
        arr_img->length_lid = *link;
        return respond(q_true, 0);
    }
}

bool append_resolve(Instance *to, Instance *with, Instance *res) {
    struct ArrayImage *img = NULL;
    Instance *T = NULL;
    if (to->image)
        img = to->image;
    else if (with->image)
        img = with->image;
    else if (res->image)
        img = res->image;
    else
        return false;
    T = img->arr_el;
    if (! to->image) to->image = laure_create_array_u(T);
    if (! with->image) with->image = laure_create_array_u(T);
    if (! res->image) res->image = laure_create_char_u(T);
    return true;
}

qresp array_predicate_append(preddata *pd, control_ctx *cctx) {
    Instance *to = pd_get_arg(pd, 0);
    Instance *with = pd_get_arg(pd, 1);
    Instance *res = pd->resp;

    if (! append_resolve(to, with, res))
        return respond(q_error, "cannot resolve; add hints");

    if (instantiated(to) && instantiated(with) && instantiated(res)) {
        MUST_BE(LENGTH(to->image) + LENGTH(with->image) == LENGTH(res->image));
        struct ArrayImage *res_img = (struct ArrayImage*) res->image;

        uint length = res_img->i_data.length;
        array_linked_t *linked = res_img->i_data.linked;

        uint check_length = LENGTH(to->image);
        array_linked_t *check_linked = LINKED(to->image);

        while (linked && length) {
            Instance *ins = linked->data;
            Instance *ins2 = check_linked->data;

            MUST_BE(image_equals(ins->image, ins2->image));

            length--;
            linked = linked->next;

            check_linked = check_linked->next;
            check_length--;

            if (length && (! check_linked || ! check_length)) {
                check_linked = LINKED(with->image);
                check_length = LENGTH(with->image);
            }
        }
        return RESPOND_TRUE;
    } else if (instantiated(to) && instantiated(with)) {
        // concatenate `to` and `with` arrays into `res` array
        uint res_length = LENGTH(to->image) + LENGTH(with->image);
        struct ArrayImage *res_img = (struct ArrayImage*)res->image;
        MUST_BE(int_domain_check_int(res_img->u_data.length, (int)res_length));

        uint i = 0;
        bool t = false;
        array_linked_t *head = NULL;
        array_linked_t *linked = NULL;
        array_linked_t *cp_linked = LINKED(to->image);

        if (! cp_linked) {
            cp_linked = LINKED(with->image);
            t = true;
        }

        while (i < res_length && cp_linked) {

            array_linked_t *old = linked;

            linked = laure_alloc(sizeof(array_linked_t));
            linked->data = cp_linked->data;
            linked->next = NULL;
            
            if (old)
                old->next = linked;
            
            if (head == NULL)
                head = linked;

            cp_linked = cp_linked->next;

            if (! cp_linked && LINKED(with->image) && ! t) {
                cp_linked = LINKED(with->image);
                t = true;
            }
            i++;
        }
        int_domain_free(res_img->u_data.length);
        res_img->state = I;
        res_img->i_data.length = res_length;
        res_img->i_data.linked = head;
        return RESPOND_TRUE;
    } else if (instantiated(to) && instantiated(res)) {
        struct ArrayImage *with_img = (struct ArrayImage*) with->image;
        uint with_length = LENGTH(res->image) - LENGTH(to->image);
        MUST_BE(int_domain_check_int(with_img->u_data.length, (int)with_length));

        // check that array offirst n elements of `res` equal to `to` array
        uint orig_res_len = LENGTH(res->image);
        LENGTH(res->image) = LENGTH(to->image);
        MUST_BE(image_equals(res->image, to->image));
        // link `with` array to linked at n's position
        LENGTH(res->image) = orig_res_len;
        array_linked_t *linked = LINKED(res->image);
        for (uint i = 0; (i < LENGTH(to->image) && linked); i++) linked = linked->next;
        with_img->state = I;
        with_img->i_data.length = with_length;
        with_img->i_data.linked = linked;
        return RESPOND_TRUE;
    } else if (instantiated(with) && instantiated(res)) {
        struct ArrayImage *to_img = (struct ArrayImage*) to->image;
        uint to_length = LENGTH(res->image) - LENGTH(with->image);
        MUST_BE(int_domain_check_int(to_img->u_data.length, to_length));

        // check array of last `to_length` elements of `res` array equal to `with` array
        array_linked_t *orig_linked = LINKED(res->image);
        uint orig_length = LENGTH(res->image);
        array_linked_t *linked = orig_linked;
        for (uint i = 0; (i < to_length && linked); i++) linked = linked->next;

        LINKED(res->image) = linked;
        LENGTH(res->image) = LENGTH(with->image);
        MUST_BE_F(image_equals(res->image, with->image), {LINKED(res->image) = orig_linked; LENGTH(res->image) = orig_length;});
        LINKED(res->image) = orig_linked;
        LENGTH(res->image) = orig_length;

        // link `to` array to `to_length` elements of `res` array
        to_img->state = I;
        to_img->i_data.length = to_length;
        to_img->i_data.linked = orig_linked;
        return RESPOND_TRUE;
    } else if (instantiated(res)) {
        // combinations
        uint length = LENGTH(res->image);
        array_linked_t *linked = LINKED(res->image);
        bool found = false;

        for (uint to_len = 0; to_len <= length; to_len++) {
            STATE(to->image) = I;
            STATE(with->image) = I;
            LENGTH(to->image) = to_len;
            LINKED(to->image) = LINKED(res->image);
            LENGTH(with->image) = length - to_len;
            LINKED(with->image) = linked;

            // passing control
            laure_scope_t *nscope = laure_scope_create_copy(cctx, pd->scope);

            laure_scope_t *old_sc = pd->scope;

            qcontext temp[1];
            *temp = qcontext_temp(cctx->qctx->next, NULL, cctx->qctx->bag);

            qcontext *old_qc = cctx->qctx;
            cctx->qctx = temp;
            cctx->scope = nscope;

            qresp result = laure_start(cctx, NULL);
            if (result.state == q_true || (result.state == q_yield && result.payload == (void*)1)) found = true;

            cctx->scope = old_sc;
            cctx->qctx = old_qc;
            laure_scope_free(nscope);

            if (! linked) break;
            linked = linked->next;
        }
        return respond(q_yield, (void*)found);
    }
    printf("too ambig\n");
    return RESPOND_FALSE;
}

int on_use(laure_session_t *session) {
    laure_api_add_predicate(
        session, "each", 
        array_predicate_each, 
        1, "ARRAY:_", "_", false, 
        DOC_each
    );
    
    laure_api_add_predicate(
        session, "__by_idx", 
        array_predicate_by_idx, 
        2, "ARRAY:_ idx:int", "_", false, 
        DOC_by_idx
    );
    
    laure_api_add_predicate(
        session, "length", 
        array_predicate_length, 
        1, "ARRAY:!", "int", false, 
        DOC_length
    );
    laure_api_add_predicate(
        session, "__append",
        array_predicate_append,
        2, "ARRAY:_ ARRAY:_", "_", false,
        NULL
    );
    return 0;
}