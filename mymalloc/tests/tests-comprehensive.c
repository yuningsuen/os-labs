// Comprehensive test cases for mymalloc/myfree
// Tests cover edge cases, concurrent operations, and stress testing

#include <testkit.h>
#include <pthread.h>
#include <mymalloc.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

// Test basic functionality
SystemTest(basic_allocation, ((const char *[]){})) {
    void *p1 = mymalloc(16);
    tk_assert(p1 != NULL, "malloc(16) should not return NULL");
    
    void *p2 = mymalloc(32);
    tk_assert(p2 != NULL, "malloc(32) should not return NULL");
    tk_assert(p1 != p2, "different malloc calls should return different pointers");
    
    myfree(p1);
    myfree(p2);
}

// Test edge cases
SystemTest(edge_cases, ((const char *[]){})) {
    // Test zero allocation
    void *p0 = mymalloc(0);
    tk_assert(p0 == NULL, "malloc(0) should return NULL");
    
    // Test very small allocation
    void *p1 = mymalloc(1);
    tk_assert(p1 != NULL, "malloc(1) should not return NULL");
    
    // Test very large allocation (should fail gracefully)
    void *p_large = mymalloc(SIZE_MAX);
    tk_assert(p_large == NULL, "malloc(SIZE_MAX) should return NULL");
    
    // Test NULL free (should not crash)
    myfree(NULL);
    myfree(p0);  // freeing NULL pointer
    
    myfree(p1);
}

// Test memory alignment
SystemTest(alignment_check, ((const char *[]){})) {
    for (int i = 1; i <= 64; i++) {
        void *ptr = mymalloc(i);
        tk_assert(ptr != NULL, "allocation should succeed");
        
        uintptr_t addr = (uintptr_t)ptr;
        tk_assert((addr & 7) == 0, "pointer should be 8-byte aligned");
        
        // Write to the memory to ensure it's accessible
        memset(ptr, 0xAA, i);
        
        myfree(ptr);
    }
}

// Test memory reuse after free
SystemTest(memory_reuse, ((const char *[]){})) {
    void *ptrs[10];
    
    // Allocate multiple blocks
    for (int i = 0; i < 10; i++) {
        ptrs[i] = mymalloc(64);
        tk_assert(ptrs[i] != NULL, "allocation should succeed");
    }
    
    // Free all blocks
    for (int i = 0; i < 10; i++) {
        myfree(ptrs[i]);
    }
    
    // Allocate again - should reuse memory
    void *new_ptr = mymalloc(64);
    tk_assert(new_ptr != NULL, "reallocation should succeed");
    
    // Check if we got one of the previous pointers (memory reuse)
    int found_reuse = 0;
    for (int i = 0; i < 10; i++) {
        if (new_ptr == ptrs[i]) {
            found_reuse = 1;
            break;
        }
    }
    tk_assert(found_reuse, "memory should be reused after free");
    
    myfree(new_ptr);
}

// Test coalescing of adjacent free blocks
SystemTest(block_coalescing, ((const char *[]){})) {
    // Allocate three consecutive blocks
    void *p1 = mymalloc(64);
    void *p2 = mymalloc(64);
    void *p3 = mymalloc(64);
    
    tk_assert(p1 && p2 && p3, "all allocations should succeed");
    
    // Free middle block first
    myfree(p2);
    
    // Free first block - should coalesce with middle
    myfree(p1);
    
    // Free third block - should coalesce with the combined block
    myfree(p3);
    
    // Now allocate a large block that should fit in the coalesced space
    void *large = mymalloc(180);  // Should fit in coalesced 3*64 + overhead
    tk_assert(large != NULL, "large allocation should succeed after coalescing");
    
    myfree(large);
}

// Test pattern writing and verification
SystemTest(data_integrity, ((const char *[]){})) {
    const size_t size = 1024;
    void *ptr = mymalloc(size);
    tk_assert(ptr != NULL, "allocation should succeed");
    
    unsigned char *data = (unsigned char *)ptr;
    
    // Write a pattern
    for (size_t i = 0; i < size; i++) {
        data[i] = (unsigned char)(i & 0xFF);
    }
    
    // Verify pattern
    for (size_t i = 0; i < size; i++) {
        tk_assert(data[i] == (unsigned char)(i & 0xFF), "data should be preserved");
    }
    
    myfree(ptr);
}

// Test multiple sizes
SystemTest(various_sizes, ((const char *[]){})) {
    void *ptrs[20];
    size_t sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    // Allocate various sizes
    for (int i = 0; i < num_sizes; i++) {
        ptrs[i] = mymalloc(sizes[i]);
        tk_assert(ptrs[i] != NULL, "allocation should succeed");
        
        // Write pattern to verify memory
        memset(ptrs[i], i + 1, sizes[i]);
    }
    
    // Verify patterns
    for (int i = 0; i < num_sizes; i++) {
        unsigned char *data = (unsigned char *)ptrs[i];
        for (size_t j = 0; j < sizes[i]; j++) {
            tk_assert(data[j] == (unsigned char)(i + 1), "data should be preserved");
        }
    }
    
    // Free in reverse order
    for (int i = num_sizes - 1; i >= 0; i--) {
        myfree(ptrs[i]);
    }
}

// Concurrent allocation test data
typedef struct {
    int thread_id;
    int num_operations;
    void **allocations;
    size_t *sizes;
    int *results;
} thread_data_t;

void *concurrent_allocator(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    
    for (int i = 0; i < data->num_operations; i++) {
        size_t size = 16 + (i % 1000);  // Sizes from 16 to 1015
        data->sizes[i] = size;
        
        void *ptr = mymalloc(size);
        data->allocations[i] = ptr;
        data->results[i] = (ptr != NULL) ? 1 : 0;
        
        if (ptr) {
            // Write a pattern to verify memory integrity
            memset(ptr, data->thread_id + 1, size);
        }
        
        // Occasionally free some previously allocated memory
        if (i > 10 && (i % 7) == 0) {
            int free_idx = i - 5;
            if (data->allocations[free_idx]) {
                myfree(data->allocations[free_idx]);
                data->allocations[free_idx] = NULL;
            }
        }
    }
    
    return NULL;
}

// Test concurrent allocations
SystemTest(concurrent_allocations, ((const char *[]){})) {
    const int num_threads = 4;
    const int ops_per_thread = 100;
    
    pthread_t threads[num_threads];
    thread_data_t thread_data[num_threads];
    
    // Initialize thread data
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].num_operations = ops_per_thread;
        thread_data[i].allocations = malloc(ops_per_thread * sizeof(void *));
        thread_data[i].sizes = malloc(ops_per_thread * sizeof(size_t));
        thread_data[i].results = malloc(ops_per_thread * sizeof(int));
        
        tk_assert(thread_data[i].allocations && thread_data[i].sizes && thread_data[i].results,
                  "test setup should succeed");
    }
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        int ret = pthread_create(&threads[i], NULL, concurrent_allocator, &thread_data[i]);
        tk_assert(ret == 0, "thread creation should succeed");
    }
    
    // Wait for threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Verify results and clean up
    int total_successful = 0;
    for (int i = 0; i < num_threads; i++) {
        for (int j = 0; j < ops_per_thread; j++) {
            if (thread_data[i].results[j]) {
                total_successful++;
                
                // Verify memory pattern if still allocated
                if (thread_data[i].allocations[j]) {
                    unsigned char *data = (unsigned char *)thread_data[i].allocations[j];
                    unsigned char expected = (unsigned char)(i + 1);
                    
                    // Check first few bytes
                    for (int k = 0; k < 10 && k < thread_data[i].sizes[j]; k++) {
                        tk_assert(data[k] == expected, "memory data should be preserved");
                    }
                    
                    myfree(thread_data[i].allocations[j]);
                }
            }
        }
        
        free(thread_data[i].allocations);
        free(thread_data[i].sizes);
        free(thread_data[i].results);
    }
    
    tk_assert(total_successful > num_threads * ops_per_thread * 0.8,
              "most allocations should succeed");
}

// Stress test with rapid alloc/free
void *stress_worker(void *arg) {
    int *iterations = (int *)arg;
    
    for (int i = 0; i < *iterations; i++) {
        size_t size = 1 + (i % 512);
        void *ptr = mymalloc(size);
        
        if (ptr) {
            // Write some data
            memset(ptr, i & 0xFF, size);
            
            // Sometimes hold onto it longer
            if ((i % 10) != 0) {
                myfree(ptr);
            } else {
                // Free it after a few more iterations
                for (int j = 0; j < 3; j++) {
                    size_t temp_size = 1 + (j % 64);
                    void *temp = mymalloc(temp_size);
                    if (temp) myfree(temp);
                }
                myfree(ptr);
            }
        }
    }
    
    return NULL;
}

SystemTest(stress_test, ((const char *[]){})) {
    const int num_threads = 8;
    const int iterations = 200;
    
    pthread_t threads[num_threads];
    int thread_iterations[num_threads];
    
    // Create stress test threads
    for (int i = 0; i < num_threads; i++) {
        thread_iterations[i] = iterations;
        int ret = pthread_create(&threads[i], NULL, stress_worker, &thread_iterations[i]);
        tk_assert(ret == 0, "thread creation should succeed");
    }
    
    // Wait for all threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // If we get here without crashing, the stress test passed
    tk_assert(1, "stress test completed without crashes");
}

// Test memory exhaustion
SystemTest(memory_exhaustion, ((const char *[]){})) {
    void *ptrs[1000];
    int count = 0;
    
    // Allocate until we fail
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = mymalloc(1024);
        if (ptrs[i] == NULL) {
            break;
        }
        count++;
    }
    
    tk_assert(count > 0, "should be able to allocate at least some memory");
    tk_assert(count < 1000, "should eventually fail when memory is exhausted");
    
    // Free everything we allocated
    for (int i = 0; i < count; i++) {
        myfree(ptrs[i]);
    }
    
    // Should be able to allocate again after freeing
    void *new_ptr = mymalloc(1024);
    tk_assert(new_ptr != NULL, "should be able to allocate after freeing");
    myfree(new_ptr);
}
