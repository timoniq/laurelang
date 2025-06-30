#include "memguard.h"
#include "laureimage.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Forward declaration for instance cleanup helper */
void instance_full_free(Instance *instance);

/* Global statistics */
static memguard_stats_t g_stats = {0};

/* =================================
 * Cleanup Context Implementation
 * ================================= */

void cleanup_ctx_init(cleanup_ctx_t *ctx) {
    if (!ctx) return;
    ctx->ptrs = NULL;
    ctx->destructors = NULL;
    ctx->count = 0;
    ctx->capacity = 0;
}

void cleanup_ctx_register(cleanup_ctx_t *ctx, void *ptr, void (*destructor)(void*)) {
    if (!ctx || !ptr) return;
    
    if (ctx->count >= ctx->capacity) {
        size_t new_capacity = ctx->capacity == 0 ? 8 : ctx->capacity * 2;
        ctx->ptrs = laure_realloc(ctx->ptrs, new_capacity * sizeof(void*));
        ctx->destructors = laure_realloc(ctx->destructors, new_capacity * sizeof(void(*)(void*)));
        ctx->capacity = new_capacity;
    }
    
    ctx->ptrs[ctx->count] = ptr;
    ctx->destructors[ctx->count] = destructor ? destructor : laure_free;
    ctx->count++;
    
    g_stats.cleanup_contexts_used++;
}

void cleanup_register_instance(cleanup_ctx_t *ctx, Instance *instance) {
    if (!ctx || !instance) return;
    
    /* Register a custom destructor for instances that frees both image and instance */
    cleanup_ctx_register(ctx, instance, (void(*)(void*))instance_full_free);
}

void cleanup_register_image(cleanup_ctx_t *ctx, void *image) {
    if (!ctx || !image) return;
    
    cleanup_ctx_register(ctx, image, (void(*)(void*))image_free);
}

void cleanup_ctx_destroy(cleanup_ctx_t *ctx) {
    if (!ctx) return;
    
    for (size_t i = 0; i < ctx->count; i++) {
        if (ctx->ptrs[i] && ctx->destructors[i]) {
            ctx->destructors[i](ctx->ptrs[i]);
        }
    }
    
    if (ctx->ptrs) laure_free(ctx->ptrs);
    if (ctx->destructors) laure_free(ctx->destructors);
    
    ctx->ptrs = NULL;
    ctx->destructors = NULL;
    ctx->count = 0;
    ctx->capacity = 0;
}

/* Helper function to properly free instance with its image */
void instance_full_free(Instance *instance) {
    if (!instance) return;
    
    if (instance->image) {
        image_free(instance->image);
    }
    laure_free(instance);
    g_stats.instances_freed++;
}

/* =================================
 * Reference Counted Images Implementation
 * ================================= */

ref_image_t *ref_image_create(void *data, void (*destructor)(void*), int image_type) {
    if (!data) return NULL;
    
    ref_image_t *img = laure_alloc(sizeof(ref_image_t));
    if (!img) return NULL;
    
    img->data = data;
    img->ref_count = 1;
    img->destructor = destructor ? destructor : image_free;
    img->image_type = image_type;
    
    g_stats.ref_images_created++;
    return img;
}

ref_image_t *ref_image_retain(ref_image_t *img) {
    if (!img) return NULL;
    
    img->ref_count++;
    return img;
}

void ref_image_release(ref_image_t *img) {
    if (!img) return;
    
    img->ref_count--;
    if (img->ref_count <= 0) {
        if (img->data && img->destructor) {
            img->destructor(img->data);
        }
        laure_free(img);
        g_stats.ref_images_freed++;
    }
}

void *ref_image_get_data(ref_image_t *img) {
    return img ? img->data : NULL;
}

/* =================================
 * Safe String Management Implementation
 * ================================= */

safe_string_t safe_string_own(const char *src) {
    safe_string_t str;
    if (src) {
        str.data = strdup(src);
        str.ownership = STRING_OWNED;
    } else {
        str.data = NULL;
        str.ownership = STRING_BORROWED;
    }
    return str;
}

safe_string_t safe_string_borrow(const char *src) {
    safe_string_t str;
    str.data = (char*)src;  /* Cast away const for interface compatibility */
    str.ownership = STRING_BORROWED;
    return str;
}

void safe_string_free(safe_string_t *str) {
    if (!str) return;
    
    if (str->ownership == STRING_OWNED && str->data) {
        laure_free(str->data);
    }
    str->data = NULL;
    str->ownership = STRING_BORROWED;
}

const char *safe_string_cstr(const safe_string_t *str) {
    return str ? str->data : NULL;
}

/* =================================
 * Arena Allocator Implementation
 * ================================= */

arena_t *arena_create(size_t block_size) {
    arena_t *arena = laure_alloc(sizeof(arena_t));
    if (!arena) return NULL;
    
    arena->current = NULL;
    arena->block_size = block_size > 0 ? block_size : 4096;  /* Default 4KB blocks */
    arena->total_allocated = 0;
    
    return arena;
}

static arena_block_t *arena_new_block(arena_t *arena, size_t min_size) {
    size_t block_size = arena->block_size;
    if (min_size > block_size) {
        block_size = min_size;  /* Allocate larger block if needed */
    }
    
    arena_block_t *block = laure_alloc(sizeof(arena_block_t));
    if (!block) return NULL;
    
    block->data = laure_alloc(block_size);
    if (!block->data) {
        laure_free(block);
        return NULL;
    }
    
    block->size = block_size;
    block->used = 0;
    block->next = arena->current;
    
    return block;
}

void *arena_alloc(arena_t *arena, size_t size) {
    if (!arena || size == 0) return NULL;
    
    /* Align to pointer size */
    size = (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
    
    /* Check if current block has enough space */
    if (!arena->current || arena->current->used + size > arena->current->size) {
        arena_block_t *new_block = arena_new_block(arena, size);
        if (!new_block) return NULL;
        arena->current = new_block;
    }
    
    void *ptr = arena->current->data + arena->current->used;
    arena->current->used += size;
    arena->total_allocated += size;
    
    g_stats.arena_bytes_allocated += size;
    return ptr;
}

void arena_destroy(arena_t *arena) {
    if (!arena) return;
    
    arena_block_t *block = arena->current;
    while (block) {
        arena_block_t *next = block->next;
        laure_free(block->data);
        laure_free(block);
        block = next;
    }
    
    laure_free(arena);
}

/* =================================
 * Enhanced Memory Statistics Implementation
 * ================================= */

memguard_stats_t memguard_get_stats(void) {
    /* Update leak count */
    g_stats.current_leaks = (g_stats.instances_created - g_stats.instances_freed) +
                           (g_stats.images_created - g_stats.images_freed) +
                           (g_stats.ref_images_created - g_stats.ref_images_freed);
    
    return g_stats;
}

void memguard_reset_stats(void) {
    memset(&g_stats, 0, sizeof(g_stats));
}

void memguard_print_report(void) {
    memguard_stats_t stats = memguard_get_stats();
    
    printf("\n=== Memory Guard Report ===\n");
    printf("Instances: %zu created, %zu freed, %zu leaked\n", 
           stats.instances_created, stats.instances_freed, 
           stats.instances_created - stats.instances_freed);
    printf("Images: %zu created, %zu freed, %zu leaked\n", 
           stats.images_created, stats.images_freed,
           stats.images_created - stats.images_freed);
    printf("Ref Images: %zu created, %zu freed, %zu leaked\n", 
           stats.ref_images_created, stats.ref_images_freed,
           stats.ref_images_created - stats.ref_images_freed);
    printf("Arena bytes allocated: %zu\n", stats.arena_bytes_allocated);
    printf("Cleanup contexts used: %zu\n", stats.cleanup_contexts_used);
    printf("Total current leaks: %zu\n", stats.current_leaks);
    printf("========================\n\n");
}

/* =================================
 * Ownership-Clear Instance Functions Implementation
 * ================================= */

Instance *instance_new_move(safe_string_t name, void *image) {
    Instance *instance = instance_new(safe_string_cstr(&name), NULL, image);
    if (instance) {
        g_stats.instances_created++;
        /* If name was owned, we transfer ownership to the instance */
        if (name.ownership == STRING_OWNED) {
            /* The instance now owns the name string */
            safe_string_t borrowed_name = safe_string_borrow(name.data);
            name = borrowed_name;  /* Prevent double-free */
        }
    }
    return instance;
}

Instance *instance_copy_from(const Instance *src) {
    if (!src) return NULL;
    
    Instance *copy = instance_deepcopy(src->name, (Instance*)src);
    if (copy) {
        g_stats.instances_created++;
    }
    return copy;
}

Instance *instance_create_with_ref_image(safe_string_t name, ref_image_t *image) {
    if (!image) return NULL;
    
    /* Retain the reference image */
    ref_image_retain(image);
    
    Instance *instance = instance_new_move(name, ref_image_get_data(image));
    if (!instance) {
        ref_image_release(image);  /* Release on failure */
        return NULL;
    }
    
    /* Store reference to ref_image in instance for proper cleanup */
    /* Note: This requires modifying Instance struct to store ref_image pointer */
    
    return instance;
}

/* =================================
 * Debug Mode Implementation
 * ================================= */

#ifdef MEMGUARD_DEBUG
typedef struct debug_alloc {
    void *ptr;
    size_t size;
    const char *file;
    int line;
    struct debug_alloc *next;
} debug_alloc_t;

static debug_alloc_t *g_debug_allocs = NULL;

void *memguard_alloc_debug(size_t size, const char *file, int line) {
    void *ptr = laure_alloc(size);
    if (!ptr) return NULL;
    
    debug_alloc_t *entry = malloc(sizeof(debug_alloc_t));  /* Use system malloc for debug info */
    if (entry) {
        entry->ptr = ptr;
        entry->size = size;
        entry->file = file;
        entry->line = line;
        entry->next = g_debug_allocs;
        g_debug_allocs = entry;
    }
    
    g_stats.total_allocations++;
    return ptr;
}

void memguard_free_debug(void *ptr, const char *file, int line) {
    if (!ptr) return;
    
    /* Find and remove from debug list */
    debug_alloc_t **current = &g_debug_allocs;
    while (*current) {
        if ((*current)->ptr == ptr) {
            debug_alloc_t *to_free = *current;
            *current = (*current)->next;
            free(to_free);  /* Use system free for debug info */
            break;
        }
        current = &(*current)->next;
    }
    
    laure_free(ptr);
    g_stats.total_frees++;
}

void memguard_dump_leaks(void) {
    printf("\n=== Memory Leak Report ===\n");
    debug_alloc_t *current = g_debug_allocs;
    int leak_count = 0;
    
    while (current) {
        printf("LEAK: %zu bytes at %p (allocated at %s:%d)\n", 
               current->size, current->ptr, current->file, current->line);
        current = current->next;
        leak_count++;
    }
    
    if (leak_count == 0) {
        printf("No leaks detected!\n");
    } else {
        printf("Total leaks: %d\n", leak_count);
    }
    printf("========================\n\n");
}
#endif