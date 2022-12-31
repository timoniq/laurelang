#include "standard.h"

DECLARE(laure_contraint_union) {
    Instance *t1 = ARGN(0);
    Instance *t2 = ARGN(1);
    Instance *union_ins = RESP_ARG;

    if (union_ins->image != NULL) {
        // know union type, resolve elements
        
        if (t1->image != NULL && t2->image != NULL) {
            RESPOND_ERROR(internal_err, NULL, "union impact");
        }
        cast_image(union_im, laure_union_image) union_ins->image;
        t1->image = image_deepcopy(union_im->A->image);
        t2->image = image_deepcopy(union_im->B->image);
    } else if (t1->image != NULL && t2->image != NULL) {
        // resolve union type

        void *im1 = image_deepcopy(t1->image);
        void *im2 = image_deepcopy(t2->image);
        
        laure_union_image *uimg = laure_union_create(
            instance_deepcopy_with_image(cctx->scope, t1->name, t1, im1),
            instance_deepcopy_with_image(cctx->scope, t2->name, t2, im2)
        );
        union_ins->image = uimg;
        union_ins->repr = union_repr;
    } else {
        RESPOND_ERROR(too_broad_err, NULL, "at least two relation elements must be known");
    }

    return True;
}