/*
 * Memory Guard Demo - Demonstrates safe memory management in laurelang
 * 
 * This demo shows how the new memory management system prevents leaks
 * and provides better error handling.
 */

#include "../src/laurelang.h"
#include "../src/memguard.h"
#include <stdio.h>

/* Example of old vs new error-prone function */

// OLD WAY - Prone to memory leaks
Instance *create_complex_instance_old(const char *name, int complexity) {
    Instance *base = instance_new(strdup(name), NULL, NULL);
    if (!base) return NULL;
    
    for (int i = 0; i < complexity; i++) {
        char *sub_name = laure_alloc(strlen(name) + 10);
        snprintf(sub_name, strlen(name) + 10, "%s_sub_%d", name, i);
        
        Instance *sub = instance_new(sub_name, NULL, NULL);
        if (!sub) {
            // MEMORY LEAK: base and previous sub instances not cleaned up!
            return NULL;
        }
        
        // Simulate some complex operation that might fail
        if (i == 3 && complexity > 5) {
            // MEMORY LEAK: base, sub, and all previous allocations leaked!
            return NULL;
        }
    }
    
    return base;
}

// NEW WAY - Memory safe with cleanup context
Instance *create_complex_instance_safe(const char *name, int complexity) {
    cleanup_ctx_t cleanup;
    cleanup_ctx_init(&cleanup);
    
    safe_string_t safe_name = safe_string_own(name);
    Instance *base = instance_new_move(safe_name, NULL);
    if (!base) {
        cleanup_ctx_destroy(&cleanup);
        return NULL;
    }
    cleanup_register_instance(&cleanup, base);
    
    for (int i = 0; i < complexity; i++) {
        char *sub_name = laure_alloc(strlen(name) + 10);
        cleanup_register(&cleanup, sub_name);  /* Auto-cleanup string */
        
        snprintf(sub_name, strlen(name) + 10, "%s_sub_%d", name, i);
        
        safe_string_t safe_sub_name = safe_string_borrow(sub_name);
        Instance *sub = instance_new_move(safe_sub_name, NULL);
        if (!sub) {
            cleanup_ctx_destroy(&cleanup);  /* Cleans up everything automatically */
            return NULL;
        }
        cleanup_register_instance(&cleanup, sub);
        
        // Simulate some complex operation that might fail
        if (i == 3 && complexity > 5) {
            cleanup_ctx_destroy(&cleanup);  /* Cleans up everything automatically */
            return NULL;
        }
    }
    
    /* Success - transfer ownership, prevent cleanup */
    cleanup.count = 0;
    cleanup_ctx_destroy(&cleanup);
    return base;
}

/* Arena allocator demo for temporary objects */
void arena_demo(void) {
    printf("=== Arena Allocator Demo ===\n");
    
    arena_t *arena = arena_create(1024);  /* 1KB blocks */
    
    /* Allocate lots of temporary strings */
    for (int i = 0; i < 100; i++) {
        char *temp_str = arena_alloc(arena, 50);
        snprintf(temp_str, 50, "temporary_string_%d", i);
        /* No need to free individual strings! */
    }
    
    printf("Allocated 100 temporary strings in arena\n");
    
    /* One call frees everything */
    arena_destroy(arena);
    printf("Arena destroyed - all memory freed automatically\n\n");
}

/* Reference counting demo */
void ref_counting_demo(void) {
    printf("=== Reference Counting Demo ===\n");
    
    /* Create a shared image with reference counting */
    void *raw_image = laure_alloc(100);  /* Simulate image data */
    ref_image_t *shared_image = ref_image_create(raw_image, laure_free, 1);
    
    printf("Created ref image with count: 1\n");
    
    /* Multiple instances can share the same image */
    ref_image_t *ref1 = ref_image_retain(shared_image);
    ref_image_t *ref2 = ref_image_retain(shared_image);
    ref_image_t *ref3 = ref_image_retain(shared_image);
    
    printf("Image now has reference count: 4\n");
    
    /* Release references - image persists until all are released */
    ref_image_release(ref1);
    printf("After first release, image still exists\n");
    
    ref_image_release(ref2);
    ref_image_release(ref3);
    printf("After more releases, image still exists\n");
    
    ref_image_release(shared_image);
    printf("After final release, image automatically freed\n\n");
}

int main(void) {
    printf("laurelang Memory Guard Demo\n");
    printf("===========================\n\n");
    
    /* Reset statistics */
    memguard_reset_stats();
    
    /* Demo arena allocator */
    arena_demo();
    
    /* Demo reference counting */
    ref_counting_demo();
    
    /* Demo old vs new instance creation */
    printf("=== Memory Safety Comparison ===\n");
    
    printf("Creating complex instance (old way, will leak on failure)...\n");
    Instance *old_result = create_complex_instance_old("test", 6);  /* Will fail and leak */
    printf("Old way result: %s\n", old_result ? "SUCCESS" : "FAILED (leaked memory!)");
    
    printf("Creating complex instance (new way, memory safe)...\n");
    Instance *new_result = create_complex_instance_safe("test", 6);  /* Will fail but not leak */
    printf("New way result: %s\n", new_result ? "SUCCESS" : "FAILED (no memory leaked!)");
    
    /* Print memory statistics */
    memguard_print_report();
    
    return 0;
}