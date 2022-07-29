#include "standard.h"
#include "laureimage.h"

DECLARE(laure_predicate_bag) {
    Instance *from = pd_get_arg(pd, 0);
    Instance *to = pd_get_arg(pd, 1);
    Instance *size_optional = pd_get_arg(pd, 2);

    if (! instantiated(from) && ! instantiated(to)) {
        RESPOND_ERROR(too_broad_err, NULL, "bag should have instantiated either `from` or `to`");
    } else if (instantiated(to)) {
        cast_image(ary_im, struct ArrayImage) to->image;
        array_linked_t *linked = ary_im->i_data.linked;
        size_t length = ary_im->i_data.length;
        bool found = true;

        while (linked && length) {
            Instance *i = linked->data;
            void *copy = image_deepcopy(cctx->scope, from->image);
            bool r = image_equals(copy, i->image);
            
            if (! r) {
                image_free(copy);
                return False;
            }
            void *old = from->image;
            from->image = copy;

            laure_scope_t *nscope = laure_scope_create_copy(cctx, pd->scope);
                    
            qcontext temp[1];
            *temp = qcontext_temp(cctx->qctx->next, NULL);

            laure_scope_t *old_sc = pd->scope;
            qcontext *old_qc = cctx->qctx;
            cctx->qctx = temp;
            cctx->scope = nscope;
            
            qresp result = laure_start(cctx, cctx->qctx ? cctx->qctx->expset : NULL);

            laure_scope_free(nscope);
            image_free(copy);
            from->image = old;
            cctx->scope = old_sc;
            cctx->qctx = old_qc;

            if (result.state == q_true || (result.state == q_yield && result.payload == (void*)1)) found = true;
            else if (result.state == q_stop || result.state == q_error) {
                return result;
            }

            linked = linked->next;
            length--;
        }
        return RESPOND_YIELD((void*)found);
    } else {

        cast_image(to_img, struct ArrayImage) to->image;

        if (to_img->state == I) {
            RESPOND_ERROR(signature_err, NULL, "bag `to` should be laure_free array");
        }

        //! fixme remove bags

        ensure_bag(cctx->qctx->next);
        Bag *bag = cctx->qctx->next->bag;
        Pocket *pocket = pocket_get_or_new(bag, pd_get_arg_link(pd, 1));

        if (size_optional)
            if (instantiated(size_optional)) {
                cast_image(sz_im, struct IntImage) size_optional->image;
                bigint bi[1];
                bigint_init(bi);
                bigint_from_int(bi, pocket->size);
                bool inv = bigint_cmp(bi, sz_im->i_data) >= 0;
                bigint_free(bi);
                if (inv)
                    return False;
            }

        if (! pocket->first) {
            array_linked_t *first = laure_alloc(sizeof(array_linked_t));
            first->data = instance_deepcopy(cctx->scope, MOCK_NAME, from);
            first->next = NULL;

            pocket->first = first;
            pocket->last = first;
            pocket->size = 1;
        } else {
            array_linked_t *new = laure_alloc(sizeof(array_linked_t));
            new->data = instance_deepcopy(cctx->scope, MOCK_NAME, from);
            new->next = NULL;
            pocket->last->next = new;
            pocket->last = new;
            pocket->size++;
        }

        int_domain_free(to_img->u_data.length);
        to_img->state = I;
        to_img->i_data.length = pocket->size;
        to_img->i_data.linked = pocket->first;
        from->doc = MARKER_NODELETE;

        if (size_optional) {
            cast_image(sz_im, struct IntImage) size_optional->image;
            bigint bi[1];
            bigint_init(bi);
            bigint_from_int(bi, pocket->size);
            if (instantiated(size_optional)) {
                bool inv = bigint_cmp(bi, sz_im->i_data) == 0;
                bigint_free(bi);
                if (! inv)
                    return False;
            } else {
                sz_im->state = I;
                sz_im->i_data = laure_alloc(sizeof(bigint));
                *(sz_im->i_data) = *bi;
            }
        }

        return True;
    }
}