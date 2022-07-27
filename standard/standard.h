#ifndef STANDARD_H
#define PREDICATES_H

#include "laurelang.h"
#include "laureimage.h"
#include "predpub.h"

qresp laure_predicate_integer_plus(preddata*, control_ctx*);
qresp laure_predicate_integer_multiply(preddata*, control_ctx*);
qresp laure_constraint_gt(preddata*, control_ctx*);
qresp laure_constraint_gte(preddata*, control_ctx*);
qresp laure_predicate_sqrt(preddata*, control_ctx*);

qresp laure_predicate_message(preddata*, control_ctx*);
qresp laure_predicate_repr(preddata*, control_ctx*);
qresp laure_predicate_format(preddata*, control_ctx*);

qresp laure_predicate_bag(preddata*, control_ctx*);

#define DECLARE(name) \
        qresp name(preddata *pd, control_ctx *cctx) 

#define from_boolean(boolean) respond(boolean ? q_true : q_false, NULL)
#define True respond(q_true, NULL)
#define False respond(q_false, NULL)
#define cast_image(name, type) type *name = (type*)
#define array_assign(ary_im, i_data) \
        

#define out(...) printf(__VA_ARGS__)

struct BuiltinPred {
    string name;
    qresp (*pred)(preddata*, control_ctx*);
    int argc; // -1 means any
    string doc;
    struct BuiltinPredHint hint;
    bool is_constraint;
};

#endif