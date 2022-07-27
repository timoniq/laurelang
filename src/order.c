#include "laurelang.h"
#include "laureimage.h"
#include "predpub.h"

typedef struct laure_mask {
    bool mask[32];
    bool response;
    size_t argc;
} laure_mask;

typedef struct mock_variable {
    string *names;
    size_t names_n;
    bool instantiated;
} mock_variable;

typedef struct ordering_scope_mock {
    mock_variable variables[256];
    size_t count;
} ordering_scope_mock;

void set_initted_variable(ordering_scope_mock *mock, string name) {
    for (size_t i = 0; i < mock->count; i++) {
        mock_variable var = mock->variables[i];
        for (size_t j = 0; j < var.names_n; j++) {
            if (str_eq(name, var.names[j])) {
                mock->variables[i].instantiated = 1;
                return;
            }
        }
    }
    mock_variable var;
    var.names = malloc(sizeof(string*));
    var.names[0] = name;
    var.names_n = 1;
    var.instantiated = 1;

    mock->variables[mock->count++] = var;
}

bool get_mock_var_is_instantiated(ordering_scope_mock *mock, string name) {
    for (size_t i = 0; i < mock->count; i++) {
        mock_variable var = mock->variables[i];
        for (size_t j = 0; j < var.names_n; j++) {
            if (str_eq(name, var.names[j])) {
                return mock->variables[i].instantiated;
            }
        }
    }
    return false;
}

bool link_name(ordering_scope_mock *mock, string link1, string link2) {
    for (size_t i = 0; i < mock->count; i++) {
        mock_variable var = mock->variables[i];
        for (size_t j = 0; j < var.names_n; j++) {
            if (str_eq(link1, var.names[j])) {
                mock->variables[i].names = realloc(
                    mock->variables[i].names, 
                    sizeof(string*) * (mock->variables[i].names_n + 1)
                );
                mock->variables[i].names[mock->variables[i].names_n++] = link2;
                return true;
            } else if (str_eq(link2, var.names[j])) {
                mock->variables[i].names = realloc(
                    mock->variables[i].names, 
                    sizeof(string*) * (mock->variables[i].names_n + 1)
                );
                mock->variables[i].names[mock->variables[i].names_n++] = link1;
                return true;
            }
        }
    }
    return false;
}

double get_initialization(ordering_scope_mock *mock, laure_expression_t *expr) {
    switch (expr->t) {
        case let_pred_call: {
            double b = 0;
            laure_expression_t *argptr;
            EXPSET_ITER(expr->ba->set, argptr, {
                if (argptr->t == let_var) {
                    if (get_mock_var_is_instantiated(mock, argptr->s))
                        b++;
                }
            });
            return b / (double)laure_expression_get_count(expr->ba->set);
        }
        case let_var: {
            return get_mock_var_is_instantiated(mock, expr->s);
        }
        default: break;
    }
    return 1;
}

laure_expression_t *get_most_initted(
    ordering_scope_mock *mock, 
    laure_expression_set **set_ptr
) {
    laure_expression_set *set = *set_ptr;

    if (! set->next) {
        *set_ptr = NULL;
        return set->expression;
    } else if (set->expression->t == let_name) {
        laure_expression_t *expr = set->expression;
        *set_ptr = set->next;
        free(set);
        return expr;
    }

    double max_initialization = 0;
    laure_expression_t *max_exp = NULL;
    size_t max_idx = 0;

    laure_expression_t *ptr;
    size_t idx = 0;
    EXPSET_ITER(set, ptr, {
        double b = get_initialization(mock, ptr);
        if (max_exp == NULL || b > max_initialization) {
            max_initialization = b;
            max_exp = ptr;
            max_idx = idx;
        }
        idx++;
    });
    if (! max_exp) return NULL;

    // minimal found, unlinking from set
    if (max_idx == 0) {
        *set_ptr = set->next;
    } else {
        laure_expression_set *p = set;
        laure_expression_set *s = set->next;
        for (size_t i = 1; i < max_idx; i++) {s = s->next; p = p->next;};
        free(s);
        p->next = s->next;
    }
    return max_exp;
}

void apply_knowledge_to_mock(ordering_scope_mock *mock, laure_expression_t *expr) {
    switch (expr->t) {
        case let_pred_call: {
            laure_expression_t *argptr;
            EXPSET_ITER(expr->ba->set, argptr, {
                if (argptr->t == let_var) {
                    set_initted_variable(mock, argptr->s);
                }
            });
            break;
        }
        case let_assert: {
            laure_expression_t *l = expr->ba->set->expression;
            laure_expression_t *r = expr->ba->set->next->expression;
            if (l->t == let_var) {
                set_initted_variable(mock, l->s);
            }
            if (r->t == let_var) {
                set_initted_variable(mock, r->s);
            }
            break;
        }
        case let_unify: {
            set_initted_variable(mock, expr->s);
            break;
        }
        case let_name: {
            link_name(mock, expr->ba->set->expression->s, expr->ba->set->next->expression->s);
            break;
        }
        default: break;
    }
}

void mock_init_with_mask(ordering_scope_mock *mock, laure_mask mask) {
    mock->count = 0;
    if (mask.response) {
        set_initted_variable(mock, laure_get_respn());
    } else {
        mock_variable var;
        var.instantiated = false;
        var.names = malloc(sizeof(string*));
        var.names[0] = laure_get_respn();
        var.names_n = 1;
        mock->variables[mock->count++] = var;
    }
    for (size_t i = 0; i < mask.argc; i++) {
        bool is_instantiated = mask.mask[i];
        mock_variable var;
        var.names_n = 1;
        var.names = malloc(sizeof(string*));
        var.names[0] = laure_get_argn(i);
        var.instantiated = is_instantiated;
        mock->variables[mock->count++] = var;
    }
}

laure_expression_set *generate_linked_permutation(
    laure_mask mask,
    laure_expression_set *unlinked_set
) {
    laure_expression_set *copy = laure_expression_set_copy(unlinked_set);
    laure_expression_set *ordered_lset = NULL;
    ordering_scope_mock mock[1];
    mock_init_with_mask(mock, mask);

    while (copy) {
        laure_expression_t *expr = get_most_initted(mock, &copy);
        ordered_lset = laure_expression_set_link(ordered_lset, expr);
        apply_knowledge_to_mock(mock, expr);
    }
    return ordered_lset;
}

void go_binary_tree(
    size_t argc,
    size_t idx,
    bool *mask,
    bool response,
    bintree_permut *bintree,
    laure_expression_set *unlinked
) {
    if (idx == argc) {
        bintree->_0 = NULL;
        bintree->_1 = NULL;
        laure_mask m;
        m.argc = argc;
        m.response = response;
        for (size_t i = 0; i < argc; i++)
            m.mask[i] = mask[i];
        bintree->set = generate_linked_permutation(m, unlinked);
    } else {
        bintree->set = NULL;
        mask[idx] = 0;
        bintree->_0 = malloc(sizeof(bintree_permut));
        go_binary_tree(argc, idx + 1, mask, response, bintree->_0, unlinked);
        mask[idx] = 1;
        bintree->_1 = malloc(sizeof(bintree_permut));
        go_binary_tree(argc, idx + 1, mask, response, bintree->_1, unlinked);
    }
}

predicate_linked_permutations laure_generate_final_permututations(
    laure_expression_set *unlinked,
    size_t argc,
    bool has_resp
) {
    if ((argc < 2 && ! has_resp) || argc == 0) {
        predicate_linked_permutations plp;
        plp.fixed = true;
        plp.fixed_set = unlinked;
        return plp;
    }
    bintree_permut *btree = malloc(sizeof(bintree_permut));
    bool *mask = malloc(sizeof(bool) * argc);
    btree->set = NULL;
    if (has_resp) {
        btree->_1 = malloc(sizeof(bintree_permut));
        go_binary_tree(argc, 0, mask, true, btree->_1, unlinked);
    } else {
        btree->_1 = NULL;
    }
    btree->_0 = malloc(sizeof(bintree_permut));
    go_binary_tree(argc, 0, mask, false, btree->_0, unlinked);
    predicate_linked_permutations plp;
    plp.fixed = false;
    plp.tree = btree;
    return plp;
}

predicate_linked_permutations laure_generate_final_fixed(laure_expression_set *set) {
    predicate_linked_permutations plp;
    plp.fixed = true;
    plp.fixed_set = set;
    return plp;
}

laure_expression_set *laure_get_ordered_predicate_body(
    predicate_linked_permutations plp, 
    size_t argc,
    bool *argi,
    bool resi
) {
    if (plp.fixed)
        return plp.fixed_set;
    
    bintree_permut *btree;
    if (resi) {
        assert(plp.tree->_1);
        btree = plp.tree->_1;
    } else {
        btree = plp.tree->_0;
    }
    for (size_t i = 0; i < argc; i++) {
        bool inst = argi[i];
        if (inst)
            btree = btree->_1;
        else
            btree = btree->_0;
    }
    assert(! btree->_0 && ! btree->_1);
    return btree->set;
}