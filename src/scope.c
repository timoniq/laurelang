// smart scope for backtracking
// copyright Arseny Kriuchkov, 2022

#include "laurelang.h"
#include "laureimage.h"

uint HEAP_RUNTIME_ID = 0;

#ifdef SCOPE_LINKED

linked_scope_t *laure_scope_insert(
    laure_scope_t *scope,
    Instance *ptr
) {
    linked_scope_t *linked = laure_alloc(sizeof(linked_scope_t));
    linked->next = scope->linked;
    linked->ptr = ptr;
    linked->link = laure_scope_generate_link();
    scope->linked = linked;
    return linked;
}

linked_scope_t *search_glob_or_null(
    laure_scope_t *scope, 
    bool (*checker)(linked_scope_t*, void*),
    void *payload,
    bool search_glob
) {
    if (search_glob && scope != scope->glob) {
        // global instances are neither 
        // backtracked nor changed (exc special cases)
        return laure_scope_find(scope->glob, checker, payload, false, false);
    }
    return NULL;
}

linked_scope_t *laure_scope_find(
    laure_scope_t *scope, 
    bool (*checker)(linked_scope_t*, void*),
    void *payload,
    bool copy,
    bool search_glob
) {
    // trying to find in current pos
    laure_scope_iter(scope, ptr, {
        if (checker(ptr, payload)) {
            return ptr;
        }
    });
    if (scope->tmp) {
        // if this is subscope, trying searching
        // in scope parent;
        linked_scope_t *tmp_ptr = laure_scope_find(
            scope->tmp, 
            checker, payload, 
            false, search_glob
        );

        if (! tmp_ptr) return NULL;

        if (copy) {
            Instance *cp = instance_deepcopy(tmp_ptr->ptr->name, tmp_ptr->ptr);
            // copying in current pos to backtrack
            // further.
            return laure_scope_insert_l(scope, cp, tmp_ptr->link);
        } else {
            return tmp_ptr;
        }
    }
    return search_glob_or_null(scope, checker, payload, search_glob);
}

// find shortcuts

bool linked_chk_key(linked_scope_t *linked, string key) {
    return streq(linked->ptr->name, key);
}

bool linked_chk_link(linked_scope_t *linked, ulong *ptr_link) {
    return linked->link == *ptr_link;
}

Instance *laure_scope_find_by_key(laure_scope_t *scope, string key, bool search_glob) {
    linked_scope_t *l = laure_scope_find(scope, linked_chk_key, key, true, search_glob);
    return l ? l->ptr : NULL;
}

Instance *laure_scope_find_by_key_l(laure_scope_t *scope, string key, ulong *link, bool search_glob) {
    linked_scope_t *l = laure_scope_find(scope, linked_chk_key, key, true, search_glob);
    if (! l) return NULL;
    if (link) *link = l->link;
    return l->ptr;
}

Instance *laure_scope_find_by_link(laure_scope_t *scope, ulong link, bool search_glob) {
    ulong li[1];
    li[0] = link;
    linked_scope_t *l = laure_scope_find(scope, linked_chk_link, li, true, search_glob);
    return l ? l->ptr : NULL;
}

// change shortcuts

struct change_pd {
    union {
        string key;
        ulong link;
    };
    ulong new_link;
};

bool linked_chk_key_change_lid(linked_scope_t *linked, struct change_pd *payload) {
    if (! streq(linked->ptr->name, payload->key)) return false;
    linked->link = payload->new_link;
    return true;
}

bool linked_chk_lid_change_lid(linked_scope_t *linked, struct change_pd *payload) {
    if (linked->link != payload->link) return false;
    linked->link = payload->new_link;
    return true;
}

Instance *laure_scope_change_link_by_key(laure_scope_t *scope, string key, ulong new_link, bool search_glob) {
    struct change_pd pd;
    pd.key = key;
    pd.new_link = new_link;
    return laure_scope_find(scope, linked_chk_key_change_lid, &pd, true, search_glob);
} 

Instance *laure_scope_change_link_by_link(laure_scope_t *scope, ulong link, ulong new_link, bool search_glob) {
    struct change_pd pd;
    pd.link = link;
    pd.new_link = new_link;
    return laure_scope_find(scope, linked_chk_lid_change_lid, &pd, true, search_glob);
}

void add_grab(ulong link, laure_scope_t *from, laure_scope_t *to) {
    Instance *from_ins = laure_scope_find_by_link(from, link, true);
    if (! from_ins) return;
    Instance *to_ins = instance_deepcopy(from_ins->name, from_ins);
    laure_scope_insert_l(to, to_ins, link);
}

// creates a dependant copy
// element will be copied from main scope
// each time the request to find one appears
laure_scope_t *laure_scope_create_copy(control_ctx *cctx, laure_scope_t *scope) {
    laure_scope_t *nscope = laure_alloc(sizeof(laure_scope_t));
    nscope->idx = scope->idx + 1;
    nscope->repeat = scope->repeat;
    nscope->glob = scope->glob;
    nscope->linked = NULL;
    nscope->tmp = scope;
    nscope->nlink = scope->nlink;
    nscope->next = scope->next;
    nscope->owner = scope->owner;
    // grab_linked_iter(cctx, add_grab, scope, nscope);
    return nscope;
}

void laure_scope_show(laure_scope_t *scope) {
    printf("--- scope linked impl ---\n");
    printf("=== ID: %zi ISTMP: %d ===\n", scope->idx, scope->tmp != 0);
    laure_scope_iter(scope, element, {
        string repr = element->ptr->repr(element->ptr);
        printf("%lu: %s%s%s %s\n", element->link, BOLD_WHITE, element->ptr->name, NO_COLOR, repr);
        laure_free(repr);
    });
    printf("---\n");
}

laure_scope_t *laure_scope_create_global() {
    laure_scope_t *scope = laure_alloc(sizeof(laure_scope_t));
    scope->idx = 1;

    if (! LAURE_LINK_ID) {
        scope->nlink = laure_alloc(sizeof(unsigned long));
        *scope->nlink = 1;
        LAURE_LINK_ID = scope->nlink;
    } else
        scope->nlink = LAURE_LINK_ID;
    
    scope->linked = NULL;
    scope->repeat = 0;
    scope->tmp = NULL;
    scope->glob = scope;
    scope->next = NULL;
    scope->owner = NULL;
    return scope;
}

laure_scope_t *laure_scope_new(laure_scope_t *global, laure_scope_t *next) {
    laure_scope_t *scope = laure_alloc(sizeof(laure_scope_t));
    scope->idx = next->idx + 1;
    scope->nlink = global->nlink;
    scope->glob = global;
    scope->linked = NULL;
    scope->tmp = NULL;
    scope->repeat = 0;
    scope->next = next;
    scope->owner = NULL;
    return scope;
}

void laure_scope_free_linked(linked_scope_t *linked) {
    if (! linked) return;
    if (linked->next) laure_scope_free_linked(linked->next);
    laure_free(linked);
}

void laure_scope_free(laure_scope_t *scope) {
    if (! scope) return;
    laure_scope_free_linked(scope->linked);
    laure_free(scope);
}

linked_scope_t *laure_scope_insert_l(
    laure_scope_t *scope,
    Instance *ptr,
    ulong link
) {
    if (! scope) return NULL;
    linked_scope_t *linked = laure_alloc(sizeof(linked_scope_t));
    linked->next = scope->linked;
    linked->ptr = ptr;
    linked->link = link;
    scope->linked = linked;
    return linked;
}

#else

laure_cell NONE_CELL = {NULL, 0, 0};

laure_cell laure_scope_insert(
    laure_scope_t *scope,
    Instance *ptr
) {
    if (scope->count == max_cells - 1) {
        printf("limit of %d elements in scope exceeded.\n", max_cells);
        exit(0);
    }
    laure_cell cell;
    cell.ptr = ptr;
    cell.link = laure_scope_generate_link();
    cell.idx = scope->count;
    scope->cells[scope->count++] = cell;
    return cell;
}

laure_cell laure_scope_find(
    laure_scope_t *scope, 
    bool (*checker)(laure_cell, void*),
    void *payload,
    bool copy,
    bool search_glob
) {
    laure_scope_iter(scope, cellptr, {
        if (checker(*cellptr, payload)) {
            return *cellptr;
        }
    });
    if (scope->glob != scope && search_glob) {
        return laure_scope_find(scope->glob, checker, payload, copy, false);
    }
    return NONE_CELL;
}

bool cell_chk_key(laure_cell cell, string key) {
    return str_eq(cell.ptr->name, key);
}

bool cell_chk_link(laure_cell cell, ulong *ptr_link) {
    return cell.link == *ptr_link;
}

Instance *laure_scope_find_by_key(laure_scope_t *scope, string key, bool search_glob) {
    laure_cell cell = laure_scope_find(scope, cell_chk_key, key, false, search_glob);
    if (cell.ptr == NULL) return NULL;
    return cell.ptr;
}

Instance *laure_scope_find_by_key_l(laure_scope_t *scope, string key, ulong *link, bool search_glob) {
    laure_cell cell = laure_scope_find(scope, cell_chk_key, key, false, search_glob);
    if (cell.ptr == NULL) {
        *link = 0;
        return NULL;
    }
    *link = cell.link;
    return cell.ptr;
}

Instance *laure_scope_find_by_link(laure_scope_t *scope, ulong link, bool search_glob) {
    laure_cell cell = laure_scope_find(scope, cell_chk_link, &link, false, search_glob);
    if (cell.ptr == NULL) return NULL;
    return cell.ptr;
}

struct change_pd {
    laure_scope_t *scope;
    union {
        string key;
        ulong link;
    };
    ulong new_link;
};

bool cell_chk_key_change_lid(laure_cell cell, struct change_pd *payload) {
    if (! str_eq(cell.ptr->name, payload->key)) return false;
    payload->scope->cells[cell.idx].link = payload->new_link;
    return true;
}

bool cell_chk_lid_change_lid(laure_cell cell, struct change_pd *payload) {
    if (cell.link != payload->link) return false;
    payload->scope->cells[cell.idx].link = payload->new_link;
    return true;
}

Instance *laure_scope_change_link_by_key(laure_scope_t *scope, string key, ulong new_link, bool search_glob) {
    struct change_pd pd;
    pd.key = key;
    pd.new_link = new_link;
    pd.scope = scope;
    laure_cell cell = laure_scope_find(scope, cell_chk_key_change_lid, &pd, false, search_glob);
    if (cell.link == 0) return NULL;
    return cell.ptr;
}

Instance *laure_scope_change_link_by_link(laure_scope_t *scope, ulong link, ulong new_link, bool search_glob) {
    struct change_pd pd;
    pd.link = link;
    pd.new_link = new_link;
    pd.scope = scope;
    laure_cell cell = laure_scope_find(scope, cell_chk_lid_change_lid, &pd, false, search_glob);
    if (cell.link == 0) return NULL;
    return cell.ptr;
}

// creates a deep copy
laure_scope_t *laure_scope_create_copy(control_ctx *cctx, laure_scope_t *scope) {
    laure_scope_t *nscope = laure_alloc(sizeof(laure_scope_t));
    nscope->next = scope->next;
    nscope->count = scope->count;
    nscope->glob = scope->glob;
    nscope->idx = scope->idx;
    nscope->nlink = scope->nlink;
    nscope->repeat = scope->repeat;
    nscope->owner = scope->owner;
    for (uint idx = 0; idx < scope->count; idx++) {
        nscope->cells[idx].idx = idx;
        nscope->cells[idx].link = scope->cells[idx].link;
        nscope->cells[idx].ptr = instance_deepcopy(scope->cells[idx].ptr->name, scope->cells[idx].ptr);
    }
    return nscope;
}

void laure_scope_show(laure_scope_t *scope) {
    printf("--- scope stodgy impl ---\n");
    printf("=== ID: %u COUNT: %u ===\n", scope->idx, scope->count);
    laure_scope_iter(scope, cellptr, {
        string repr = cellptr->ptr->repr(cellptr->ptr);
        if (! cellptr->ptr->locked)
            printf("%lu: %s%s%s %s\n", cellptr->link, BOLD_WHITE, cellptr->ptr->name, NO_COLOR, repr);
        else
            printf("%lu: %s%s%s%s %s\n", cellptr->link, BOLD_DEC, BLUE_COLOR, cellptr->ptr->name, NO_COLOR, repr);
        laure_free(repr);
    });
    printf("---\n");
}

laure_scope_t *laure_scope_create_global() {
    laure_scope_t *scope = laure_alloc(sizeof(laure_scope_t));
    scope->idx = 1;
    scope->count = 0;

    if (! LAURE_LINK_ID) {
        scope->nlink = laure_alloc(sizeof(unsigned long));
        *scope->nlink = 1;
        LAURE_LINK_ID = scope->nlink;
    } else
        scope->nlink = LAURE_LINK_ID;

    scope->glob = scope;
    scope->next = NULL;
    scope->repeat = 0;
    return scope;
}

laure_scope_t *laure_scope_new(laure_scope_t *global, laure_scope_t *next) {
    laure_scope_t *scope = laure_alloc(sizeof(laure_scope_t));
    scope->idx = next ? next->idx + 1 : global->idx + 1;
    scope->count = 0;
    scope->glob = global;
    scope->next = next;
    scope->nlink = global->nlink;
    scope->repeat = 0;
    scope->owner = NULL;
    return scope;
}

void laure_scope_free_linked(laure_cell cell) {}

laure_cell laure_scope_insert_l(
    laure_scope_t *scope,
    Instance *ptr,
    ulong link
) {
    if (! scope) return NONE_CELL;
    if (scope->count == max_cells - 1) {
        printf("limit of %d elements in scope exceeded.\n", max_cells);
        exit(0);
    }
    laure_cell cell;
    cell.ptr = ptr;
    cell.link = link;
    cell.idx = scope->count;
    scope->cells[scope->count++] = cell;
    return cell;
}

void laure_scope_free(laure_scope_t *scope) {
    for (uint idx = 0; idx < scope->count; idx++) {
        Instance *instance = scope->cells[idx].ptr;
        image_free(instance->image);
        laure_free(instance);
    }
    laure_free(scope);
}

#endif

ulong laure_scope_generate_link() {
    if (! LAURE_LINK_ID) {
        LAURE_LINK_ID = laure_alloc(sizeof(ulong));
        *LAURE_LINK_ID = (unsigned long)1;
    }
    ulong link = *LAURE_LINK_ID;
    *LAURE_LINK_ID = *LAURE_LINK_ID + 1;
    return link;
}

string laure_scope_generate_unique_name() {
    ulong link = laure_scope_generate_link();
    char buff[16];
    snprintf(buff, 16, "$%lu", link);
    return strdup( buff );
}

string laure_scope_get_owner(laure_scope_t *scope) {
    return scope->owner;
}

Instance *laure_scope_find_var(laure_scope_t *scope, laure_expression_t *var, bool search_glob) {
    if (var->s == NULL && var->flag2) {
        if (var->flag2 >= VAR_LINK_LIMIT) {
            // access from heap
            uint heap_id = VAR_LINK_LIMIT - var->flag2;
            assert(heap_id < ID_MAX);
            return HEAP_TABLE[heap_id];
        }
        return laure_scope_find_by_link(scope, var->flag2, search_glob);
    }
    assert(var->s);
    return laure_scope_find_by_key(scope, var->s, search_glob);
}

ulong laure_set_heap_value(Instance *value, uint id) {
    assert(id < ID_MAX);
    HEAP_TABLE[id] = value;
    return (ulong)(VAR_LINK_LIMIT + id);
}

/*
Instance *laure_search_heap_value_by_name(string name) {
    for (uint i = LAURE_COMPILER_ID_OFFSET; i < ID_MAX; i++) {
        if (! HEAP_TABLE[i]) continue;
        else if (str_eq(HEAP_TABLE[i]->name, name)) return HEAP_TABLE[i];
    }
    return NULL;
}
*/

/*
void laure_heap_add_value(Instance *value) {
    HEAP_TABLE[LAURE_COMPILER_ID_OFFSET + HEAP_RUNTIME_ID++] = value;
}
*/