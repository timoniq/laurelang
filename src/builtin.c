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

Instance *get_hint(string hint, laure_scope_t *scope) {
    Instance *arg;
    if (str_eq(hint, "!"))
        arg = NULL;
    else if (str_eq(hint, "_"))
        arg = instance_new(MOCK_NAME, NULL, NULL);
    else arg = laure_scope_find_by_key(scope, hint, true);
    return arg;
}

void laure_register_builtins(laure_session_t *session) {
    laure_scope_t *scope = session->scope;

    for (int i = 0; i < (sizeof(BUILTIN_INSTANCES) / sizeof(struct Builtin)); i++) {
        struct Builtin builtin = BUILTIN_INSTANCES[i];
        Instance *ins = malloc(sizeof(Instance));
        *ins = builtin.generate();
        ins->doc = strdup(builtin.doc);
        laure_scope_insert(scope, ins);
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

                Instance *arg = get_hint(hint_tname, scope);
                cimage->hints[j] = hint_new(hint_name, arg);
                linked1 = linked1->next;
                j++;
            } while (linked1);

        } else {
            cimage->hints = NULL;
        }

        if (builtin.hint.resp_type != NULL) {
            string hint = builtin.hint.resp_type;
            Instance *resp = get_hint(hint, scope);
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
        img->is_primitive = false;

        Instance *ins = instance_new(builtin.name, builtin.doc, img);
        ins->repr = builtin.is_constraint ? bc_repr : bp_repr;

        laure_scope_insert(scope, ins);
    }
}

Instance builtin_integer() {
    Instance instance;
    instance.name = strdup("int");
    instance.doc = NULL;
    instance.locked = true;
    instance.repr = int_repr;
    instance.image = laure_create_integer_u(
        int_domain_new()
    );
    return instance;
}