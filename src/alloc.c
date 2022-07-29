#include "laurelang.h"

#ifdef ALLOCATOR_STATS
size_t ALLOCATOR_ALLOCATED = 0;
size_t ALLOCATOR_REALLOCATED = 0;
size_t ALLOCATOR_FREED = 0;
#endif

void *laure_alloc(size_t size) {
    #ifdef ALLOCATOR_STATS
    ALLOCATOR_ALLOCATED++;
    #endif
    return malloc(size);
}

void laure_free(void *ptr) {
    #ifdef ALLOCATOR_STATS
    ALLOCATOR_FREED++;
    #endif
    free(ptr);
}

void *laure_realloc(void *ptr, size_t size) {
    #ifdef ALLOCATOR_STATS
    ALLOCATOR_REALLOCATED++;
    #endif
    return realloc(ptr, size);
}

laure_allocator_stats laure_allocator_stats_get() {
    laure_allocator_stats stats;
    #ifdef ALLOCATOR_STATS
    stats.allocated = ALLOCATOR_ALLOCATED;
    stats.reallocated = ALLOCATOR_REALLOCATED;
    stats.freed = ALLOCATOR_FREED;
    #else
    printf("Compile with -DALLOCATOR_STATS=1 to use `laure_allocator_stats_get`\n");
    stats.allocated = 0;
    stats.reallocated = 0;
    stats.freed = 0;
    #endif
    return stats;
}

void laure_allocator_reset_stats() {
    #ifdef ALLOCATOR_STATS
    ALLOCATOR_ALLOCATED = 0;
    ALLOCATOR_REALLOCATED = 0;
    ALLOCATOR_FREED = 0;
    #endif
}