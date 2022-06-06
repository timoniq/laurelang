/* Laurelang expression enums */

typedef enum {
    let_set,
    let_var,
    let_pred_call,
    let_decl,
    let_assert,
    let_image,
    let_pred,
    let_choice_1,
    let_choice_2,
    let_name,
    let_custom,
    let_constraint,
    let_struct_def,
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
    let_nope
} laure_expression_type;

typedef enum laure_command_type {
    command_setflag,
    command_use,
    command_useso
} laure_command_type;