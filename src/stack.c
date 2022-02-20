#include "laurelang.h"
#include "utils.h"
#include "laureimage.h"

#ifndef SCOPE_COUNT_LIMIT
#define SCOPE_COUNT_LIMIT 1000
#endif

laure_gc_treep_t *laure_gc_treep_create_node(laure_gc_ptr_t ptr_t, void *ptr) {
    laure_gc_treep_t *gctn = malloc(sizeof(laure_gc_treep_t));
    laure_gc_treep_t node = {ptr_t, ptr, true, NULL, NULL};
    *gctn = node;
    return gctn;
}

laure_gc_treep_t *laure_gc_treep_add(laure_gc_treep_t *gct, laure_gc_ptr_t ptr_t, void *ptr) {
    if (!ptr) return gct;

    laure_gc_treep_t *node = laure_gc_treep_create_node(ptr_t, ptr);

    if (gct == NULL) {
        return node;
    }

    laure_gc_treep_t *g = gct;

    for (;;) {
        if (ptr > g->ptr) {
            if (g->right == NULL) {
                g->right = node;
                break;
            } else {
                g = g->right;
            }
        } else if (ptr < g->ptr) {
            if (g->left == NULL) {
                g->left = node;
                break;
            } else {
                g = g->left;
            }
        } else {
            free(node);
            return gct;
        }
    }
    return gct;
}

laure_gc_treep_t *laure_gc_treep_destroy(laure_gc_treep_t *gct) {
    if (gct == NULL) return NULL;

    if (gct->left) {
        gct->left = laure_gc_treep_destroy(gct->left);
    }

    if (gct->right) {
        gct->right = laure_gc_treep_destroy(gct->right);
    }

    if (gct->active) {
        switch (gct->ptr_t) {
            case GCPTR_INSTANCE: {
                // free instance
                instance_free(gct->ptr);
                break;
            }
            case GCPTR_IMAGE: {
                // free image
                image_free(gct->ptr, true);
                break;
            }
            case GCPTR: {
                free(gct->ptr);
                break;
            }
        }
        LAURE_GC_COLLECTED++;
    }

    free(gct);
    return NULL;
}

#ifndef FEATURE_LINKED_SCOPE

laure_stack_t *laure_stack_parent() {
    laure_stack_t *stack = malloc(sizeof(laure_stack_t));

    stack->next_link_id = malloc(sizeof(long));
    (*stack->next_link_id) = 1;

    stack->next = NULL;

    stack->current.cells = malloc(SCOPE_SIZE);
    stack->current.count = 0;
    stack->current.id = 1;
    stack->global = stack;
    stack->repeat = 0;

    return stack;
};

laure_stack_t *laure_stack_init(laure_stack_t *next) {
    if (next->current.id == SCOPE_MAX_COUNT) {
        printf("%smemory_error%s: Stack overflow\n", RED_COLOR, NO_COLOR);
        exit(1);
        return NULL;
    }
    laure_stack_t *stack = malloc(sizeof(laure_stack_t));
    stack->next = next;
    stack->next_link_id = next->next_link_id;
    stack->current.cells = malloc(SCOPE_SIZE);
    stack->current.count = 0;
    stack->current.id = next->current.id + 1;
    stack->global = next->global;
    stack->repeat = 0;

    return stack;
}

Instance *laure_stack_get(laure_stack_t* stack, string key) {
    for (int i = 0; i < stack->current.count; i++) {
        Cell cell = stack->current.cells[i];
        if (cell.instance == NULL || cell.instance->name == NULL) continue;
        if (str_eq(cell.instance->name, key))
            return cell.instance;
    }
    if (stack != stack->global && stack->global) {
        return laure_stack_get(stack->global, key);
    } else {
        return NULL;
    }
}

Cell laure_stack_get_cell(laure_stack_t *stack, string key) {
    if (!stack) goto return_null_cell;
    for (int i = 0; i < stack->current.count; i++) {
        Cell cell = stack->current.cells[i];
        if (cell.instance == NULL) continue;
        if (str_eq(strdup(cell.instance->name), key))
            return cell;
    }
    if (stack != stack->global && stack->global) {
        return laure_stack_get_cell(stack->global, key);
    } else {
        return_null_cell: {
        Cell null_cell;
        null_cell.instance = NULL;
        null_cell.link_id = 0;
        return null_cell;
        }
    }
}

Cell laure_stack_get_cell_only_locals(laure_stack_t *stack, string key) {
    for (int i = 0; i < stack->current.count; i++) {
        Cell cell = stack->current.cells[i];
        if (cell.instance == NULL) continue;
        else if (str_eq(cell.instance->name, key))
            return cell;
    }
    Cell null_cell;
    null_cell.instance = NULL;
    null_cell.link_id = 0;
    return null_cell;
}

void laure_stack_change_lid_by_lid(laure_stack_t *stack, long old, long new) {
    for (int i = 0; i < stack->current.count; i++) {
        Cell cell = stack->current.cells[i];
        if (cell.instance == NULL) continue;
        else if (cell.link_id == old) {
            stack->current.cells[i].link_id = new;
            break;
        }
    }
}

void laure_stack_change_lid(laure_stack_t *stack, string key, long lid) {
    if (stack == NULL) return;

    long old_lid = 0;
    for (int i = 0; i < stack->current.count; i++) {
        Cell cell = stack->current.cells[i];
        if (cell.instance == NULL) continue;
        else if (str_eq(cell.instance->name, key)) {
            old_lid = cell.link_id;
            stack->current.cells[i].link_id = lid;
            break;
        }
    }

    if (old_lid != 0) {
        laure_stack_change_lid_by_lid(stack->next, old_lid, lid);
        laure_stack_change_lid_by_lid(stack->global, old_lid, lid);
    } 
}

Cell laure_stack_get_cell_by_lid(laure_stack_t *stack, long lid, bool allow_global) {
    for (int i = 0; i < stack->current.count; i++) {
        Cell cell = stack->current.cells[i];
        if (cell.instance == NULL) continue;
        else if (cell.link_id == lid)
            return cell;
    }
    if (stack != stack->global && stack->global) {
        return laure_stack_get_cell_by_lid(stack->global, lid, allow_global);
    } else {
        Cell null_cell;
        null_cell.instance = NULL;
        null_cell.link_id = 0;
        return null_cell;
    }
}

int laure_stack_delete(laure_stack_t* stack, string key) {
    for (int i = 0; i < stack->current.count; i++) {
        Cell cell = stack->current.cells[i];
        if (cell.instance == NULL) continue;
        else if (str_eq(cell.instance->name, key)) {
            cell.instance = NULL;
            stack->current.cells[i] = cell;
            return 1;
        }
    }
    return 0;
}

void laure_stack_insert(laure_stack_t* stack, Cell cell) {
    if ((stack->current.count + 1) * sizeof(Cell) > SCOPE_SIZE) {
        printf("%smemory_error%s: Stack scope overflow\n", RED_COLOR, NO_COLOR);
        exit(1);
    }
    stack->current.cells[stack->current.count] = cell;
    stack->current.count++;
}

string laure_stack_get_uname(laure_stack_t* stack) {
    long uid = *stack->next_link_id;
    *stack->next_link_id = *stack->next_link_id + 1;
    string uname;
    uname = malloc((sizeof(char) * 10));
    sprintf(uname, "$u%ld", uid);
    return uname;
}

long laure_stack_get_uid(laure_stack_t* stack) {
    long uid = *stack->next_link_id;
    *stack->next_link_id = *stack->next_link_id + 1;
    return uid;
}

laure_stack_t *laure_stack_clone(laure_stack_t *from, bool deep) {
    if (from == NULL) return NULL;
    // else if (from == from->global) return from;

    laure_stack_t *new = malloc(sizeof(laure_stack_t));
    new->next_link_id = from->next_link_id;
    new->current.cells = malloc(SCOPE_SIZE);
    new->current.count = 0;
    new->current.id = from->current.id + 1;
    new->global = from->global;
    new->next = (!deep) ? from->next : laure_stack_clone(from->next, true);
    new->repeat = from->repeat;

    int j = 0;
    for (int i = 0; i < from->current.count; i++) {
        Cell old_cell = from->current.cells[i];
        if (old_cell.instance == NULL) {
            continue;
        }
        Cell new_cell;
        new_cell.instance = instance_deepcopy(from, old_cell.instance->name, old_cell.instance);
        new_cell.instance->locked = old_cell.instance->locked;
        new_cell.link_id = old_cell.link_id;
        laure_stack_insert(new, new_cell);
        j++;
    }

    assert(j == new->current.count);
    return new;
}

void laure_stack_add_to(laure_stack_t *from, laure_stack_t *to) {
    for (int i = 0; i < from->current.count; i++) {
        Cell cell = from->current.cells[i];
        if (cell.instance == NULL) continue;
        bool found = false;
        for (int j = 0; j < to->current.count; j++) {
            Cell next_cell = to->current.cells[j];
            if (next_cell.instance == NULL) continue;
            if (str_eq(cell.instance->name, next_cell.instance->name)) {
                laure_gc_treep_add(GC_ROOT, GCPTR_IMAGE, next_cell.instance->image);
                next_cell.instance->image = image_deepcopy(from, cell.instance->image);
                found = true;
                break;
            }
        }
        if (!found) {
            Cell cell_copy;
            cell_copy.link_id = cell.link_id;
            cell_copy.instance = instance_deepcopy(to, strdup(cell.instance->name), cell.instance);
            laure_stack_insert(to, cell_copy);
        }
    }
}

void laure_stack_free(laure_stack_t *stack) {
    if (stack == NULL) return;
    else if (stack->next == NULL) return;

    if (stack->current.cells)

    for (int i = 0; i < stack->current.count; i++) {
        Cell cell = stack->current.cells[i];
        if (cell.instance == NULL) continue; 
        GC_ROOT = laure_gc_treep_add(GC_ROOT, GCPTR_IMAGE, cell.instance->image);
        GC_ROOT = laure_gc_treep_add(GC_ROOT, GCPTR_INSTANCE, cell.instance);
        GC_ROOT = laure_gc_treep_add(GC_ROOT, GCPTR, cell.instance->name);
    }

    laure_stack_free(stack->next);
    // GC_ROOT = laure_gc_treep_add(GC_ROOT, GCPTR, stack->current.cells);
    // free(stack);
    GC_ROOT = laure_gc_treep_add(GC_ROOT, GCPTR, stack);
}

size_t laure_stack_get_size_deep(laure_stack_t *stack) {
    if (stack == NULL) return 0;
    size_t sz = 0;
    sz = sz + sizeof(laure_stack_t);
    sz = sz + SCOPE_SIZE;
    for (int i = 0; i < stack->current.count; i++) {

        sz = sz + instance_get_size_deep(stack->current.cells->instance);
    }
    return sz;
}

void laure_stack_show(laure_stack_t* stack) {
    printf("--- stodgy stack for debug ---\n");
    printf("=== ID: %d, CNT: %d, SZ: %zu ===\n", stack->current.id, stack->current.count, laure_stack_get_size_deep(stack));
    for (int i = 0; i < stack->current.count; i++) {
        if (!stack->current.cells) continue;
        Instance *instance = stack->current.cells[i].instance;
        long link = stack->current.cells[i].link_id;
        string r = instance != NULL ? instance->repr(instance) : NULL;
        printf("%d: name=%s link=%ld value=%s doc=%s\n", i, instance == NULL ? "?" : instance->name, link, r == NULL ? "(deleted)" : r, instance == NULL ? "" : (instance->doc == NULL ? "-" : "+"));
        free(r);
    }
    printf("---\n");
}

#else

laure_stack_t *laure_stack_parent() {
    laure_stack_t *stack = malloc(sizeof(laure_stack_t));

    stack->next_link_id = malloc(sizeof(long));
    (*stack->next_link_id) = 1;

    stack->next = NULL;

    stack->current.cells = 0;
    stack->current.id = 1;

    stack->global = stack;
    stack->is_temp = false;
    stack->main = 0;

    return stack;
}

laure_stack_t *laure_stack_init(laure_stack_t *next) {
    if (next->current.id == SCOPE_COUNT_LIMIT) {
        printf("scope count limit reached\n");
        laure_trace_comment("limit reached");
        return NULL;
    }

    laure_stack_t *stack = malloc(sizeof(laure_stack_t));

    stack->next = next;
    stack->next_link_id = next->next_link_id;

    stack->current.cells = 0;
    stack->current.id = next->current.id + 1;

    stack->global = next->global;
    stack->is_temp = false;
    stack->main = 0;

    return stack;
}

Instance *laure_stack_get(laure_stack_t *stack, string key) {
    if (stack == NULL) return NULL;

    Cell cell;
    STACK_ITER(stack, cell, {
        if (str_eq(cell.instance->name, key)) {
            if (!cell.instance->image) return NULL;
            return cell.instance;
        }
    }, true);

    if (stack->is_temp) {
        Cell first = laure_stack_get_cell(stack->main, key);
        Instance *temp = instance_deepcopy(stack->main, key, first.instance);

        Cell temp_cell;
        temp_cell.instance = temp;
        temp_cell.link_id = first.link_id;

        laure_stack_insert(stack, temp_cell);
        return temp;
    } else {
        Cell cell = laure_stack_get_cell(stack, key);
        if (!cell.instance) return NULL;
        return cell.instance;
    }
}

int laure_stack_delete(laure_stack_t *stack, string key) {
    Cell cell;
    STACK_ITER(stack, cell, {
        if (str_eq(cell.instance->name, key)) {
            // dirty delete
            cell.instance = NULL;
            lcell->cell = cell;
            return 1;
        }
    }, false);
    return 0;
}

void laure_stack_insert(laure_stack_t *stack, Cell cell) {
    LinkedCell *lcell = malloc(sizeof(LinkedCell));
    lcell->cell = cell;
    lcell->link = stack->current.cells;
    stack->current.cells = lcell;
}

string laure_stack_get_uname(laure_stack_t *stack) {
    long uid = *stack->next_link_id;
    *stack->next_link_id = *stack->next_link_id + 1;
    string uname = malloc(10);
    sprintf(uname, "$u%ld", uid);
    return uname;
}

long laure_stack_get_uid(laure_stack_t *stack) {
    long uid = *stack->next_link_id;
    *stack->next_link_id = *stack->next_link_id + 1;
    return uid;
}

laure_stack_t *laure_stack_clone(laure_stack_t *from, bool deep) {
    if (!from) return NULL;
    laure_stack_t *stack = malloc(sizeof(laure_stack_t));

    stack->next_link_id = from->next_link_id;

    stack->next = (from->next) ? laure_stack_clone(from->next, true) : from->next;

    stack->current.cells = 0;
    stack->current.id = from->current.id + 1;

    stack->global = from->global;
    stack->is_temp = true;
    stack->main = from;

    return stack;
}

Cell laure_stack_get_cell(laure_stack_t *stack, string key) {
    assert(stack);

    Cell cell;
    STACK_ITER(stack, cell, {
        if (str_eq(cell.instance->name, key)) {
            if (!cell.instance->image) {
                Cell none_cell;
                none_cell.link_id = -1;
                none_cell.instance = NULL;
                return none_cell;
            }
            return cell;
        }
    }, true);

    if (stack->is_temp) {

        Cell first = laure_stack_get_cell(stack->main, key);

        if (!first.instance) {
            return laure_stack_get_cell(stack->global, key);
        }

        Instance *temp = instance_deepcopy(stack->main, key, first.instance);

        Cell temp_cell;
        temp_cell.instance = temp;
        temp_cell.link_id = first.link_id;

        laure_stack_insert(stack, temp_cell);
        return temp_cell;

    } else {

        if (stack != stack->global) {
            return laure_stack_get_cell(stack->global, key);
        }

        Cell none_cell;
        none_cell.link_id = -1;
        none_cell.instance = NULL;
        return none_cell;
    }
}

Cell laure_stack_get_cell_by_lid(laure_stack_t *stack, long lid, bool allow_global) {
    Cell cell;
    STACK_ITER(stack, cell, {
        if (cell.link_id == lid) {
            if (!cell.instance->image) {
                Cell none_cell;
                none_cell.link_id = -1;
                none_cell.instance = NULL;
                return none_cell;
            }
            return cell;
        }
    }, true);

    if (stack->is_temp) {

        Cell first = laure_stack_get_cell_by_lid(stack->main, lid, allow_global);

        if (!first.instance) {
            if (!allow_global) {
                Cell none_cell;
                none_cell.link_id = -1;
                none_cell.instance = NULL;
                return none_cell;
            }
            return laure_stack_get_cell_by_lid(stack->global, lid, allow_global);
        }

        Instance *temp = instance_deepcopy(stack->main, first.instance->name, first.instance);

        Cell temp_cell;
        temp_cell.instance = temp;
        temp_cell.link_id = lid;

        laure_stack_insert(stack, temp_cell);
        return temp_cell;

    } else {

        if (stack != stack->global && allow_global) {
            return laure_stack_get_cell_by_lid(stack->global, lid, allow_global);
        }

        Cell none_cell;
        none_cell.link_id = -1;
        none_cell.instance = NULL;
        return none_cell;
    }
}

Cell laure_stack_get_cell_only_locals(laure_stack_t *stack, string key) {
    Cell cell;
    STACK_ITER(stack, cell, {
        if (str_eq(cell.instance->name, key)) {
            if (cell.instance->image) {
                return cell;
            }
        }
    }, true);
    
    if (stack->is_temp) {

        Cell first = laure_stack_get_cell_only_locals(stack->main, key);

        if (!first.instance) {
            Cell none_cell;
            none_cell.link_id = -1;
            none_cell.instance = NULL;
            return none_cell;
        }

        Instance *temp = instance_deepcopy(stack->main, key, first.instance);

        Cell temp_cell;
        temp_cell.instance = temp;
        temp_cell.link_id = first.link_id;

        laure_stack_insert(stack, temp_cell);
        return temp_cell;
    } else {
        Cell none_cell;
        none_cell.link_id = -1;
        none_cell.instance = NULL;
        return none_cell;
    }
}

void laure_stack_show(laure_stack_t *stack) {
    printf("--- linked scope ---\n");
    printf("=== ID: %d, TMP: %s ===\n", stack->current.id, stack->is_temp ? "YES" : "NO");
    Cell cell;
    STACK_ITER(stack, cell, {
        string r = cell.instance->repr(cell.instance);
        printf("%s: %s (%ld)\n", cell.instance->name, r, cell.link_id);
        free(r);
    }, false);
    printf("---\n");
}

void laure_stack_free(laure_stack_t *stack) {
    if (stack == NULL || stack->global == stack) {
        return;
    }
    Cell cell;

    STACK_ITER(stack, cell, {
        GC_ROOT = laure_gc_treep_add(GC_ROOT, GCPTR_IMAGE, cell.instance->image);
        GC_ROOT = laure_gc_treep_add(GC_ROOT, GCPTR_INSTANCE, cell.instance);
    }, false);

    laure_stack_free(stack->next);
    laure_gc_treep_add(GC_ROOT, GCPTR, stack);
}

size_t laure_stack_get_size_deep(laure_stack_t *stack) {
    if (stack == NULL) return 0;
    size_t sz = 0;
    sz = sz + sizeof(laure_stack_t);

    Cell cell;
    STACK_ITER(stack, cell, {
        sz = sz + instance_get_size_deep(cell.instance);
        sz = sz + sizeof(Cell);
        sz = sz + sizeof(LinkedCell);
    }, false);

    return sz;
}

void laure_stack_change_lid_by_lid(laure_stack_t *stack, long old, long new) {
    Cell cell;
    STACK_ITER(stack, cell, {
        if (cell.link_id == old) {
            lcell->cell.link_id = new;
        }
    }, false);
    if (stack->is_temp) {
        laure_stack_change_lid_by_lid(stack->main, old, new);
    }
}

void laure_stack_change_lid(laure_stack_t *stack, string key, long lid) {
    Cell cell;
    long old_lid = 0;

    STACK_ITER(stack, cell, {
        if (str_eq(cell.instance->name, key)) {
            lcell->cell.link_id = lid;
        }
    }, false);

    if (stack->is_temp) {
        laure_stack_change_lid(stack->main, key, lid);
    } else {
        if (old_lid) {
            laure_stack_change_lid_by_lid(stack->next, old_lid, lid);
            laure_stack_change_lid_by_lid(stack->global, old_lid, lid);
        }
    }
}

void laure_stack_add_to(laure_stack_t *from, laure_stack_t *to) {
    printf("laure_stack_add_to not implemented\n");
}

#endif