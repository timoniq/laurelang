#include "builtin.h"
#include <stdio.h>

string bp_repr(Instance* ins) {
    char buff[64];
    snprintf(buff, 64, "(predicate %s)", ins->name);
    return strdup(buff);
}

string bc_repr(Instance* ins) {
    char buff[64];
    snprintf(buff, 64, "(constraint %s)", ins->name);
    return strdup(buff);
}

Instance *get_hint(string hint, laure_stack_t *stack) {
    Instance *arg;
    if (str_eq(hint, "!"))
        arg = NULL;
    else if (str_eq(hint, "_"))
        arg = instance_new(strdup("abstract"), NULL, NULL);
    else arg = laure_stack_get(stack, hint);
    
    return arg;
}

void laure_register_builtins(laure_session_t *session) {
     laure_stack_t *stack = session->stack;

    for (int i = 0; i < (sizeof(BUILTIN_INSTANCES) / sizeof(struct Builtin)); i++) {
        struct Builtin builtin = BUILTIN_INSTANCES[i];
        Instance *ins = malloc(sizeof(Instance));
        *ins = builtin.generate();
        ins->doc = strdup(builtin.doc);
        Cell builtin_cell;
        builtin_cell.instance = ins;
        builtin_cell.link_id = laure_stack_get_uid(stack);
        laure_stack_insert(stack, builtin_cell);
    }
    
    for (int i = 0; i < (sizeof(BUILTIN_PREDICATES) / sizeof(struct BuiltinPred)); i++) {
        struct BuiltinPred builtin = BUILTIN_PREDICATES[i];

        struct PredicateCImage *cimage = malloc(sizeof(struct PredicateCImage));

        cimage->argc = builtin.argc;
        cimage->pred = builtin.pred;

        if (builtin.hint.arg_types != NULL) {
            cimage->hints = malloc(sizeof(Instance*) * builtin.argc);

            string hint_names_s = builtin.hint.arg_types;

            string_linked *linked1 = string_split(hint_names_s, ' ');
            int j = 0;

            do {
                string hint_pattern = linked1->s;
                string_linked *linked2 = string_split(hint_pattern, ':');
                string hint_name = linked2->s;
                string hint_tname = linked2->next->s;

                Instance *arg = get_hint(hint_tname, stack);
                cimage->hints[j] = hint_new(hint_name, arg);
                j++;
                linked1 = linked1->next;
            } while (linked1);

        } else {
            cimage->hints = NULL;
        }

        if (builtin.hint.resp_type != NULL) {
            string hint = builtin.hint.resp_type;
            Instance *resp = get_hint(hint, stack);
            cimage->resp_hint = resp;
        } else {
            cimage->resp_hint = NULL;
        }

        struct PredicateImageVariation pv;
        pv.t = PREDICATE_C;
        pv.c = *cimage;

        struct PredicateImage *img = malloc(sizeof(struct PredicateImage));

        struct PredicateImageVariationSet *variations = pvs_new();
        pvs_push(variations, pv);

        img->t = !builtin.is_constraint ? PREDICATE_FACT : CONSTRAINT_FACT;
        img->variations = variations;
        img->translator = NULL;

        Instance *ins = instance_new(builtin.name, builtin.doc, img);
        ins->repr = builtin.is_constraint ? bc_repr : bp_repr;

        Cell pred_cell;
        pred_cell.instance = ins;
        pred_cell.link_id = laure_stack_get_uid(stack);

        laure_stack_insert(stack, pred_cell);
    }
}

string integer_repr(Instance *ins) {
    struct IntImage *im = (struct IntImage*)ins->image;
    char buff[512];
    if (im->state == I) {
        bigint_write(buff, 512, im->i_data);
    } else if (im->state == U) {
        strcpy(buff, int_domain_repr(im->u_data));
    }
    return strdup(buff);
}

Instance builtin_integer() {
    Instance instance;
    instance.name = strdup("int");
    instance.derived = NULL;
    instance.doc = NULL;
    instance.locked = true;
    instance.repr = integer_repr;
    instance.image = integer_u_new();
    ((struct IntImage*)instance.image)->translator = new_translator(int_translator);
    return instance;
}

Instance builtin_char() {
    struct CharImage *img = malloc(sizeof(struct CharImage));
    img->t = CHAR;
    img->is_set = false;
    img->c = 0;
    img->translator = new_translator(char_translator);

    Instance instance;
    instance.name = strdup("char");
    instance.derived = NULL;
    instance.doc = NULL;
    instance.locked = true;
    instance.repr = char_repr;
    instance.image = img;
    return instance;
}

Instance builtin_string() {
    Instance instance;
    instance.name = strdup("string");
    instance.derived = NULL;
    instance.doc = NULL;
    instance.locked = true;
    instance.repr = string_repr;
    instance.image = array_u_new(laure_stack_get(LAURE_SESSION->stack, "char"));
    ((struct ArrayImage*)instance.image)->translator = new_translator(string_translator);
    return instance;
}

Instance *laure_cle_add_predicate(
    laure_session_t *session,
    string name,
    qresp (*pred)(preddata*, control_ctx*),
    int argc,
    string args_hint,
    string response_hint,
    bool is_constraint,
    string doc
) {

    struct PredicateCImage *cimage = malloc(sizeof(struct PredicateCImage));
    cimage->argc = argc;
    cimage->pred = pred;

    if (args_hint != NULL) {
        cimage->hints = malloc(sizeof(Instance*) * argc);

        string hint_names_s = args_hint;
        cimage->hints = malloc(sizeof(Instance*) * argc);

        string_linked *linked1 = string_split(hint_names_s, ' ');
        int j = 0;

        do {
            string hint_pattern = linked1->s;
            string_linked *linked2 = string_split(hint_pattern, ':');
            string hint_name = linked2->s;
            string hint_tname = linked2->next->s;

            Instance *arg = get_hint(hint_tname, session->stack);
            cimage->hints[j] = hint_new(hint_name, arg);
            j++;
            linked1 = linked1->next;
        } while (linked1);

    } else {
        cimage->hints = NULL;
    }

    if (response_hint != NULL) {
        cimage->resp_hint = get_hint(response_hint, session->stack);
    } else {
        cimage->resp_hint = NULL;
    }

    struct PredicateImageVariation pv;
    pv.t = PREDICATE_C;
    pv.c = *cimage;

    struct PredicateImage *img = malloc(sizeof(struct PredicateImage));

    struct PredicateImageVariationSet *variations = pvs_new();
    pvs_push(variations, pv);

    img->t = !is_constraint ? PREDICATE_FACT : CONSTRAINT_FACT;
    img->variations = variations;
    img->translator = NULL;

    Instance *ins = instance_new(name, doc, img);
    ins->repr = is_constraint ? bc_repr : bp_repr;

    Cell pred_cell;
    pred_cell.instance = ins;
    pred_cell.link_id = laure_stack_get_uid(session->stack);

    laure_stack_insert(session->stack, pred_cell);
    return ins;
}
