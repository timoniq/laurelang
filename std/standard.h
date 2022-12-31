#ifndef STANDARD_H
#define STANDARD_H

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

qresp laure_predicate_each(preddata*, control_ctx*);
qresp laure_predicate_by_idx(preddata*, control_ctx*);
qresp laure_predicate_length(preddata*, control_ctx*);
qresp laure_predicate_append(preddata*, control_ctx*);

qresp laure_predicate_map(preddata*, control_ctx*);

qresp laure_contraint_union(preddata*, control_ctx*);

#define DECLARE(name) \
        qresp name(preddata *pd, control_ctx *cctx) 

#define ARGN(number) \
        pd_get_arg(pd, number)

#define RESP_ARG \
        pd->resp

#define from_boolean(boolean) respond(boolean ? q_true : q_false, NULL)
#define True respond(q_true, NULL)
#define False respond(q_false, NULL)
#define Continue respond(q_continue, NULL)
#define cast_image(name, type) type *name = (type*)
#define array_assign(ary_im, i_data) \
        

#define out(...) printf(__VA_ARGS__)

typedef struct laure_builtin_predicate {
    string name;
    qresp (*pred)(preddata*, control_ctx*);
    int argc; // -1 means any
    string doc;
    laure_builtin_predicate_hint hint;
    bool is_constraint;
} laure_builtin_predicate;

enum Rounding {
    RoundingDown,
    RoundingUp,
};

extern laure_enum_atom ROUNDING_ATOMU[];
size_t rounding_atomu_size();

#endif