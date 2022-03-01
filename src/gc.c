#include "laurelang.h"
#include "laureimage.h"

#define TRACK_MAX 65535

void *GC_IMAGES_TRACK[TRACK_MAX];
void *GC_INSTANCES_TRACK[TRACK_MAX];

uint INSTANCES_TRACKED_N = 0;
uint IMAGES_TRACKED_N = 0;

void laure_gc_check_space() {
    if (IMAGES_TRACKED_N >= TRACK_MAX) {
        printf("[OVERFLOW] gc\n");
        exit(6);
    }
}

void laure_gc_track_instance(Instance *instance) {
    GC_INSTANCES_TRACK[INSTANCES_TRACKED_N++] = instance;
    laure_gc_check_space();
}

void laure_gc_track_image(void *image) {
    GC_IMAGES_TRACK[IMAGES_TRACKED_N++] = image;
    laure_gc_check_space();
}

void laure_gc_mark_image(void *img) {
    if (! img) return;
    bool *m = (bool*)img;
    *m = ! *m;
    if (m) {
        switch (read_head(img).t) {
            case ARRAY: {
                struct ArrayImage *im = (struct ArrayImage*)img;
                // laure_gc_mark_instance(im->arr_el);
                if (im->state == I) {
                    for (int i = 0; i < im->i_data.length; i++)
                        laure_gc_mark_instance(im->i_data.array[i]);
                }
                break;
            }
            default: break;
        }
    }
}

void laure_gc_mark_instance(Instance *instance) {
    instance->mark = ! instance->mark;
    laure_gc_mark_image(instance->image);
}

void laure_gc_mark(laure_stack_t *reachable) {
    for (int idx = 0; idx < INSTANCES_TRACKED_N; idx++) {
        Instance *ins = GC_INSTANCES_TRACK[idx];
        if (! ins) continue;
        ins->mark = false;
    }
    for (int idx = 0; idx < IMAGES_TRACKED_N; idx++) {
        if (! GC_IMAGES_TRACK[idx]) continue;
        bool *m = (bool*)GC_IMAGES_TRACK[idx];
        *m = false;
    }
    Cell cell;
    STACK_ITER(reachable, cell, {
        laure_gc_mark_instance(cell.instance);
    }, false);

    if (reachable->next) {
        laure_gc_mark(reachable->next);
    } else if (reachable != reachable->global) {
        laure_gc_mark(reachable->global);
    }
}

typedef struct {Instance *ptr; uint flag;} PoolVal;

#define POOL_NONREF_SZ 16384
PoolVal POOL_NONREF[POOL_NONREF_SZ];
uint POOL_NONREF_N = 0;

void pool_add_q(Instance *ins) {
    for (int idx = 0; idx < POOL_NONREF_N; idx++) {
        if (POOL_NONREF[idx].ptr == ins) return;
    }
    PoolVal pv = {ins, 0};
    POOL_NONREF[POOL_NONREF_N++] = pv;
    if (POOL_NONREF_N >= POOL_NONREF_SZ) {
        printf("[OVERFLOW]\n");
        exit(6);
    }
}

void pool_add_f(Instance *ins) {
    for (int idx = 0; idx < POOL_NONREF_N; idx++) {
        if (POOL_NONREF[idx].ptr == ins) { POOL_NONREF[idx].flag = 1; return; }
    }
    PoolVal pv = {ins, 1};
    POOL_NONREF[POOL_NONREF_N++] = pv;
    if (POOL_NONREF_N >= POOL_NONREF_SZ) {
        printf("[OVERFLOW]\n");
        exit(6);
    }
}

void laure_image_destroy(void *img) {
    switch (read_head(img).t) {
        case INTEGER: {
            struct IntImage *im = (struct IntImage*)img;
            if (im->state == I) {
                bigint_free(im->i_data);
            } else {
                int_domain_free(im->u_data);
            }
            break;
        }
        case ARRAY: {
            struct ArrayImage *im = (struct ArrayImage*)img;
            if (im->state == I) {
                for (int idx = 0; idx < im->i_data.length; idx++)
                    pool_add_q(im->i_data.array[idx]);
            }
        }
    }
    free(img);
}

uint laure_gc_destroy() {
    uint count = 0;

    for (int idx = 0; idx < INSTANCES_TRACKED_N; idx++) {
        Instance *ins = GC_INSTANCES_TRACK[idx];
        if (! ins) continue;
        if (! ins->mark ) {
            pool_add_f(ins);
            free(ins);
            GC_INSTANCES_TRACK[idx] = 0;
            count++;
        }
    }

    for (int idx = 0; idx < IMAGES_TRACKED_N; idx++) {
        void *im = GC_IMAGES_TRACK[idx];
        if (! im) continue;
        bool mark = *(bool*)im;
        if (! mark) {
            laure_image_destroy(im);
            GC_IMAGES_TRACK[idx] = 0;
            count++;
        }
    }

    for (int idx = 0; idx < POOL_NONREF_N; idx++) {
        PoolVal pv = POOL_NONREF[idx];
        if (! pv.flag) {
            laure_image_destroy(pv.ptr->image);
            free(pv.ptr);
            count++;
        }
    }
    POOL_NONREF_N = 0;
    return count;
}

uint laure_gc_run(laure_stack_t *reachable) {
    laure_gc_mark(reachable);
    return laure_gc_destroy();
}