#include <laurelang.h>
#include <predpub.h>
#include <laureimage.h>
#include <math.h>
#include <builtin.h>

#define DOC_each "Array enumerating predicate\n```each(array) = element```\nFrom @/array pkg"
#define DOC_by_idx "Predicate of idx to element relation in array\n```array by_idx idx = element```\nFrom @/array pkg"
#define DOC_length "Array length declaration\nFrom @/array pkg"

#define TOO_AMBIG respond(q_error, strdup( "too much ambiguations" ) );

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
                if (! image_equals(el_ins->image, el->image))
                    return respond(q_false, NULL);
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
                    nscope->repeat++;

                    laure_scope_t *old_sc = pd->scope;
                    qcontext *old_qc = cctx->qctx;
                    cctx->qctx = cctx->qctx->next;
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
        bool result = image_equals(arr_img->arr_el->image, el_ins->image);
        return respond(result ? q_true : q_false, 0);
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

    if (instantiated(arr_ins)) {
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
                    nscope->repeat++;

                    laure_scope_t *old_sc = pd->scope;
                    qcontext *old_qc = cctx->qctx;
                    cctx->qctx = cctx->qctx->next;
                    cctx->scope = nscope;
                    
                    qresp result = laure_start(cctx, cctx->qctx ? cctx->qctx->expset : NULL);
                    if (result.state == q_true || (result.state == q_yield && result.payload == (void*)1)) found = true;

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
                nscope->repeat++;

                laure_scope_t *old_sc = pd->scope;
                qcontext *old_qc = cctx->qctx;
                cctx->qctx = cctx->qctx->next;
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
            arr_img->ref = realloc(arr_img->ref, sizeof(ref_element) * (arr_img->ref_count + 1));
            arr_img->ref[arr_img->ref_count++] = ref;
        } else {
            arr_img->ref = malloc(sizeof(ref_element));
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
        bool result = image_equals(len_img, real_len_img);
        return respond(result ? q_true : q_false, 0);
    } else if (instantiated(len_ins)) {
        arr_img->u_data.length->t = SINGLE;
        arr_img->u_data.length->lborder.data = len_img->i_data;
        return respond(q_true, 0);
    } else {
        ulong link[1];
        laure_scope_find_by_key_l(cctx->scope, len_ins->name, link, true);
        arr_img->length_lid = *link;
        return respond(q_true, 0);
    }
}


int on_include(laure_session_t *session) {
    laure_api_add_predicate(
        session, "each", 
        array_predicate_each, 
        1, "ARRAY:_", "_", false, 
        DOC_each
    );
    
    laure_api_add_predicate(
        session, "by_idx", 
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
    return 0;
}