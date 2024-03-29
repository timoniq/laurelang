#ifndef EXPR_H
#define EXPR_H
/* Laurelang expression enums */

typedef enum {
    let_set,
    let_name,
    let_pred_call,
    let_decl,
    let_assert,
    let_image,
    let_pred,
    let_choice_1,
    let_choice_2,
    let_rename,
    let_data,
    let_constraint,
    let_complex_data,
    let_struct,
    let_array,
    let_unify,
    let_quant,
    let_domain,
    let_imply,
    let_ref,
    let_cut,
    let_atom,
    let_command,
    let_singlq,
    let_atom_sign,
    let_auto,
    let_nested,
    let_nope
} laure_expression_type;

typedef enum laure_command_type {
    command_setflag,
    command_use,
    command_useso,
    command_error,
    command_lock_unlock
} laure_command_type;

typedef enum laure_auto_type {
    AUTO_ID,
} laure_auto_type;

typedef enum laure_compact_predicate_flag {
    pred_flag_none,
    pred_flag_primitive,
    pred_flag_template,
    pred_flag_cut,
    pred_flag_primitive_template
} laure_compact_predicate_flag;

#define PREDFLAG_IS_PRIMITIVE(flag) (flag == pred_flag_primitive || flag == pred_flag_primitive_template)
#define PREDFLAG_IS_TEMPLATE(flag) (flag == pred_flag_template || flag == pred_flag_primitive_template)
#define PREDFLAG_IS_CUT(flag) (flag == pred_flag_cut)
#define PREDFLAG_GET(is_primitive, is_template, is_cut) \
    is_cut ? pred_flag_cut : ((is_primitive && is_template) ? pred_flag_primitive_template : ((is_primitive || is_template) ? (is_primitive ? pred_flag_primitive : pred_flag_template) : pred_flag_none))

#endif