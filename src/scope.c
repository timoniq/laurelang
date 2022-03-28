// smart scope for backtracking
// copyright Arseny Kriuchkov, 2022

#include "laurelang.h"
#include "laureimage.h"

linked_scope_t *laure_scope_insert(
    laure_scope_t *scope,
    Instance *ptr
) {
    linked_scope_t *linked = malloc(sizeof(linked_scope_t));
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
            Instance *cp = instance_deepcopy(scope, tmp_ptr->ptr->name, tmp_ptr->ptr);
            // copying in current pos to backtrack
            // further.
            laure_scope_insert_l(scope, cp, tmp_ptr->link);
            return cp;
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
    linked_scope_t *l = laure_scope_find(scope, linked_chk_key, &link, true, search_glob);
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

// creates a dependant copy
// element will be copied from main scope
// each time the request to find one appears
laure_scope_t *laure_scope_create_copy(laure_scope_t *scope) {
    laure_scope_t *nscope = malloc(sizeof(laure_scope_t));
    nscope->idx = scope->idx + 1;
    nscope->repeat = scope->repeat;
    nscope->glob = scope->glob;
    nscope->linked = NULL;
    nscope->tmp = scope;
    nscope->nlink = scope->nlink;
    return nscope;
}

void laure_scope_show(laure_scope_t *scope) {
    printf("--- stack default impl ---\n");
    printf("=== ID: %zi ISTMP: %d ===\n", scope->idx, scope->tmp != 0);
    laure_scope_iter(scope, element, {
        string repr = element->ptr->repr(element->ptr);
        printf("%lu: %sname=%s%s %svalue=%s%s\n", element->link, GRAY_COLOR, NO_COLOR, element->ptr->name, GRAY_COLOR, NO_COLOR, repr);
        free(repr);
    });
    printf("---\n");
}

ulong laure_scope_generate_link() {
    assert(LAURE_LINK_ID);
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

laure_scope_t *laure_scope_create_global() {
    laure_scope_t *scope = malloc(sizeof(laure_scope_t));
    scope->idx = 1;

    if (! LAURE_LINK_ID) {
        scope->nlink = malloc(sizeof(unsigned long));
        *scope->nlink = 1;
        LAURE_LINK_ID = scope->nlink;
    } else
        scope->nlink = LAURE_LINK_ID;
    
    scope->linked = NULL;
    scope->repeat = 0;
    scope->tmp = NULL;
    scope->glob = scope;
    return scope;
}

laure_scope_t *laure_scope_new(laure_scope_t *global, laure_scope_t *next) {
    laure_scope_t *scope = malloc(sizeof(laure_scope_t));
    scope->idx = next->idx + 1;
    scope->nlink = global->nlink;
    scope->glob = global;
    scope->linked = NULL;
    scope->tmp = NULL;
    scope->repeat = 0;
    return scope;
}

void laure_scope_free_linked(linked_scope_t *linked) {
    if (linked->next) laure_scope_free_linked(linked->next);
    free(linked);
}

void laure_scope_free(laure_scope_t *scope) {
    laure_scope_free_linked(scope->linked);
    free(scope);
}

linked_scope_t *laure_scope_insert_l(
    laure_scope_t *scope,
    Instance *ptr,
    ulong link
) {
    linked_scope_t *linked = malloc(sizeof(linked_scope_t));
    linked->next = scope->linked;
    linked->ptr = ptr;
    linked->link = link;
    scope->linked = linked;
    return linked;
}