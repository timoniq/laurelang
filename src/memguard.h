#ifndef MEMGUARD_H
#define MEMGUARD_H

#include "laurelang.h"
#include <stddef.h>
// #include <stdbool.h>

/* =================================
 * Memory Guard - Safe Memory Management for laurelang
 * ================================= */

/* Cleanup Context System for Error-Safe Memory Management */
typedef struct cleanup_ctx {
    void **ptrs;
    void (**destructors)(void*);
    size_t count;
    size_t capacity;
} cleanup_ctx_t;

/* Initialize cleanup context */
void cleanup_ctx_init(cleanup_ctx_t *ctx);

/* Register pointer for cleanup with optional custom destructor */
void cleanup_ctx_register(cleanup_ctx_t *ctx, void *ptr, void (*destructor)(void*));

/* Register pointer for simple laure_free cleanup */
#define cleanup_register(ctx, ptr) cleanup_ctx_register(ctx, ptr, laure_free)

/* Register instance for proper cleanup (image + instance) */
void cleanup_register_instance(cleanup_ctx_t *ctx, Instance *instance);

/* Register image for proper cleanup */
void cleanup_register_image(cleanup_ctx_t *ctx, void *image);

/* Cleanup all registered pointers */
void cleanup_ctx_destroy(cleanup_ctx_t *ctx);

/* Helper function to properly free instance with its image */
void instance_full_free(Instance *instance);

/* Cleanup-aware error response macro */
#define RESPOND_ERROR_CLEANUP(ctx, k, exp, ...) do {\
        cleanup_ctx_destroy(ctx); \
        char _errmsg[error_max_length]; \
        snprintf(_errmsg, error_max_length, __VA_ARGS__); \
        LAURE_ACTIVE_ERROR = laure_error_create(k, strdup(_errmsg), exp); \
        return respond(q_error, (void*)1);} while (0)

/* Convenience macro to start function with cleanup context */
#define WITH_CLEANUP_CTX(name) \
    cleanup_ctx_t name; \
    cleanup_ctx_init(&name); \
    __attribute__((cleanup(cleanup_ctx_destroy))) cleanup_ctx_t *_cleanup_guard = &name;

/* Simpler macros for common patterns */
#define MEMGUARD_CLEANUP_CONTEXT(name) \
    cleanup_ctx_t name; \
    cleanup_ctx_init(&name);

#define MEMGUARD_REGISTER(ctx, ptr) cleanup_register(ctx, ptr)
#define MEMGUARD_REGISTER_INSTANCE(ctx, ptr) cleanup_register_instance(ctx, ptr)
#define MEMGUARD_REGISTER_IMAGE(ctx, ptr) cleanup_register_image(ctx, ptr)

#define MEMGUARD_CLEANUP(ctx) cleanup_ctx_destroy(ctx)

/* Transfer ownership - prevent cleanup */
#define MEMGUARD_TRANSFER_OWNERSHIP(ctx) do { (ctx)->count = 0; } while(0)

/* Safe allocation that auto-registers */
#define MEMGUARD_ALLOC(ctx, size) ({ \
    void *_ptr = laure_alloc(size); \
    if (_ptr) cleanup_register(ctx, _ptr); \
    _ptr; \
})

/* Safe string duplication */
#define MEMGUARD_STRDUP(ctx, str) ({ \
    char *_str = strdup(str); \
    if (_str) cleanup_register(ctx, _str); \
    _str; \
})

/* =================================
 * Reference Counted Images
 * ================================= */

typedef struct ref_image {
    void *data;
    int ref_count;
    void (*destructor)(void*);
    int image_type;  /* Store original image type for debugging */
} ref_image_t;

/* Create reference counted image */
ref_image_t *ref_image_create(void *data, void (*destructor)(void*), int image_type);

/* Increment reference count */
ref_image_t *ref_image_retain(ref_image_t *img);

/* Decrement reference count, free if reaches 0 */
void ref_image_release(ref_image_t *img);

/* Get the wrapped data */
void *ref_image_get_data(ref_image_t *img);

/* =================================
 * Safe String Management
 * ================================= */

/* String ownership-aware functions */
typedef enum {
    STRING_BORROWED,    /* Don't free - owned by caller */
    STRING_OWNED        /* Must free - ownership transferred */
} string_ownership_t;

typedef struct safe_string {
    char *data;
    string_ownership_t ownership;
} safe_string_t;

/* Create owned string (strdup equivalent) */
safe_string_t safe_string_own(const char *src);

/* Create borrowed string (no allocation) */
safe_string_t safe_string_borrow(const char *src);

/* Free string if owned */
void safe_string_free(safe_string_t *str);

/* Get C string data */
const char *safe_string_cstr(const safe_string_t *str);

/* =================================
 * Arena Allocator for Temporary Objects
 * ================================= */

typedef struct arena_block {
    char *data;
    size_t size;
    size_t used;
    struct arena_block *next;
} arena_block_t;

typedef struct arena {
    arena_block_t *current;
    size_t block_size;
    size_t total_allocated;
} arena_t;

/* Create new arena with specified block size */
arena_t *arena_create(size_t block_size);

/* Allocate from arena */
void *arena_alloc(arena_t *arena, size_t size);

/* Free entire arena */
void arena_destroy(arena_t *arena);

/* Convenience macro for temporary arena allocation */
#define WITH_ARENA(name, block_size) \
    arena_t *name = arena_create(block_size); \
    __attribute__((cleanup(arena_destroy))) arena_t *_arena_guard = name;

/* =================================
 * Enhanced Memory Statistics
 * ================================= */

typedef struct memguard_stats {
    size_t instances_created;
    size_t instances_freed;
    size_t images_created;
    size_t images_freed;
    size_t ref_images_created;
    size_t ref_images_freed;
    size_t arena_bytes_allocated;
    size_t cleanup_contexts_used;
    size_t total_allocations;
    size_t total_frees;
    size_t current_leaks;
} memguard_stats_t;

/* Get current memory statistics */
memguard_stats_t memguard_get_stats(void);

/* Reset statistics */
void memguard_reset_stats(void);

/* Print memory report */
void memguard_print_report(void);

/* =================================
 * Ownership-Clear Instance Functions
 * ================================= */

/* Instance creation with clear ownership semantics */
Instance *instance_new_move(safe_string_t name, void *image);  /* Takes ownership */
Instance *instance_copy_from(const Instance *src);            /* Creates new copy */
Instance *instance_create_with_ref_image(safe_string_t name, ref_image_t *image);

/* =================================
 * Debug Mode Features
 * ================================= */

#ifdef MEMGUARD_DEBUG
    /* Track allocation sources for leak detection */
    #define memguard_alloc(size) memguard_alloc_debug(size, __FILE__, __LINE__)
    #define memguard_free(ptr) memguard_free_debug(ptr, __FILE__, __LINE__)
    
    void *memguard_alloc_debug(size_t size, const char *file, int line);
    void memguard_free_debug(void *ptr, const char *file, int line);
    void memguard_dump_leaks(void);
#else
    #define memguard_alloc(size) laure_alloc(size)
    #define memguard_free(ptr) laure_free(ptr)
#endif

#endif /* MEMGUARD_H */