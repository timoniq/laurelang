#include "standard.h"
#include "laureimage.h"

DECLARE(laure_predicate_bag) {
    Instance *from = pd_get_arg(pd, 0);
    Instance *to = pd_get_arg(pd, 1);
    Instance *size_optional = pd_get_arg(pd, 2);

    if (! instantiated(from) && ! instantiated(to)) {
        RESPOND_ERROR(too_broad_err, NULL, "bag should have instantiated either `from` or `to`");
    } else if (instantiated(to)) {
        RESPOND_ERROR(not_implemented_err, NULL, "inst case");
    } else {

        cast_image(to_img, struct ArrayImage) to->image;

        if (to_img->state == I) {
            RESPOND_ERROR(signature_err, NULL, "bag `to` should be free array");
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
            array_linked_t *first = malloc(sizeof(array_linked_t));
            first->data = instance_deepcopy(cctx->scope, MOCK_NAME, from);
            first->next = NULL;

            pocket->first = first;
            pocket->last = first;
            pocket->size = 1;
        } else {
            array_linked_t *new = malloc(sizeof(array_linked_t));
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
                sz_im->i_data = malloc(sizeof(bigint));
                *(sz_im->i_data) = *bi;
            }
        }

        return True;
    }
}