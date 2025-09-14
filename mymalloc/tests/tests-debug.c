// Debug and validation tests for mymalloc/myfree

#include <testkit.h>
#include <pthread.h>
#include <mymalloc.h>
#include <stdint.h>
#include <string.h>

// Test for double-free detection (should not crash)
SystemTest(double_free_safety, ((const char *[]){})) {
    void *ptr = mymalloc(64);
    tk_assert(ptr != NULL, "allocation should succeed");
    
    // First free - should work normally
    myfree(ptr);
    
    // Second free - should not crash (undefined behavior, but should be safe)
    myfree(ptr);
    
    tk_assert(1, "double free should not crash the program");
}

// Test for invalid pointer free (should not crash)
SystemTest(invalid_free_safety, ((const char *[]){})) {
    // Try to free various invalid pointers
    char stack_var = 42;
    
    // These should not crash (though behavior is undefined)
    myfree(&stack_var);           // Stack pointer
    myfree((void*)0x12345678);    // Random address
    myfree((void*)1);             // Very low address
    
    tk_assert(1, "invalid free should not crash the program");
}

// Test memory boundaries
SystemTest(boundary_check, ((const char *[]){})) {
    void *ptr = mymalloc(100);
    tk_assert(ptr != NULL, "allocation should succeed");
    
    char *data = (char *)ptr;
    
    // Write to the beginning and end of allocated block
    data[0] = 'A';
    data[99] = 'Z';
    
    // Verify the data
    tk_assert(data[0] == 'A', "first byte should be preserved");
    tk_assert(data[99] == 'Z', "last byte should be preserved");
    
    myfree(ptr);
}

// Test allocation ordering and uniqueness
SystemTest(allocation_uniqueness, ((const char *[]){})) {
    const int num_ptrs = 100;
    void *ptrs[num_ptrs];
    
    // Allocate many blocks
    for (int i = 0; i < num_ptrs; i++) {
        ptrs[i] = mymalloc(32);
        tk_assert(ptrs[i] != NULL, "allocation should succeed");
    }
    
    // Check that all pointers are unique
    for (int i = 0; i < num_ptrs; i++) {
        for (int j = i + 1; j < num_ptrs; j++) {
            tk_assert(ptrs[i] != ptrs[j], "all allocations should be unique");
        }
    }
    
    // Check that allocations don't overlap
    for (int i = 0; i < num_ptrs; i++) {
        char *start_i = (char *)ptrs[i];
        char *end_i = start_i + 32;
        
        for (int j = i + 1; j < num_ptrs; j++) {
            char *start_j = (char *)ptrs[j];
            char *end_j = start_j + 32;
            
            // Check for overlap
            int overlap = !(end_i <= start_j || end_j <= start_i);
            tk_assert(!overlap, "allocations should not overlap");
        }
    }
    
    // Free all
    for (int i = 0; i < num_ptrs; i++) {
        myfree(ptrs[i]);
    }
}

// Test heap corruption detection
SystemTest(heap_integrity, ((const char *[]){})) {
    // Allocate several blocks with known patterns
    void *p1 = mymalloc(64);
    void *p2 = mymalloc(128);
    void *p3 = mymalloc(256);
    
    tk_assert(p1 && p2 && p3, "allocations should succeed");
    
    // Fill with patterns
    memset(p1, 0xAA, 64);
    memset(p2, 0xBB, 128);
    memset(p3, 0xCC, 256);
    
    // Free middle block
    myfree(p2);
    
    // Check that other blocks are still intact
    unsigned char *data1 = (unsigned char *)p1;
    unsigned char *data3 = (unsigned char *)p3;
    
    for (int i = 0; i < 64; i++) {
        tk_assert(data1[i] == 0xAA, "block 1 should be intact after freeing block 2");
    }
    
    for (int i = 0; i < 256; i++) {
        tk_assert(data3[i] == 0xCC, "block 3 should be intact after freeing block 2");
    }
    
    // Free remaining blocks
    myfree(p1);
    myfree(p3);
}

// Test sequential allocation/free patterns
SystemTest(sequential_patterns, ((const char *[]){})) {
    const int iterations = 1000;
    
    // Pattern 1: Sequential allocate then sequential free
    void *ptrs[10];
    
    for (int iter = 0; iter < iterations; iter++) {
        // Allocate all
        for (int i = 0; i < 10; i++) {
            ptrs[i] = mymalloc(32 + (i * 8));
            if (ptrs[i]) {
                memset(ptrs[i], i + 1, 32 + (i * 8));
            }
        }
        
        // Free all
        for (int i = 0; i < 10; i++) {
            myfree(ptrs[i]);
        }
        
        // Every 100 iterations, test reverse free order
        if ((iter % 100) == 0) {
            for (int i = 0; i < 10; i++) {
                ptrs[i] = mymalloc(48);
            }
            
            // Free in reverse order
            for (int i = 9; i >= 0; i--) {
                myfree(ptrs[i]);
            }
        }
    }
    
    tk_assert(1, "sequential patterns test completed");
}

// Test rapid allocation/deallocation
SystemTest(rapid_alloc_free, ((const char *[]){})) {
    const int rapid_iterations = 5000;
    
    for (int i = 0; i < rapid_iterations; i++) {
        size_t size = 16 + (i % 100);
        void *ptr = mymalloc(size);
        
        if (ptr) {
            // Touch the memory
            *((char*)ptr) = (char)(i & 0xFF);
            
            // Immediately free
            myfree(ptr);
        }
    }
    
    tk_assert(1, "rapid allocation/free test completed");
}

// Thread safety validation
typedef struct {
    int thread_id;
    int errors;
    int allocations;
} validation_thread_data_t;

void *validation_worker(void *arg) {
    validation_thread_data_t *data = (validation_thread_data_t *)arg;
    data->errors = 0;
    data->allocations = 0;
    
    const int ops = 500;
    void *ptrs[10];
    
    // Initialize
    for (int i = 0; i < 10; i++) {
        ptrs[i] = NULL;
    }
    
    for (int i = 0; i < ops; i++) {
        int idx = i % 10;
        
        // Free existing
        if (ptrs[idx]) {
            myfree(ptrs[idx]);
            ptrs[idx] = NULL;
        }
        
        // Allocate new
        size_t size = 32 + (data->thread_id * 100) + (i % 64);
        ptrs[idx] = mymalloc(size);
        
        if (ptrs[idx]) {
            data->allocations++;
            
            // Verify alignment
            if (((uintptr_t)ptrs[idx]) & 7) {
                data->errors++;
            }
            
            // Write and verify pattern
            char pattern = (char)(data->thread_id + i);
            memset(ptrs[idx], pattern, size);
            
            // Check first and last byte
            char *mem = (char *)ptrs[idx];
            if (mem[0] != pattern || mem[size-1] != pattern) {
                data->errors++;
            }
        }
    }
    
    // Clean up
    for (int i = 0; i < 10; i++) {
        if (ptrs[i]) {
            myfree(ptrs[i]);
        }
    }
    
    return NULL;
}

SystemTest(thread_safety_validation, ((const char *[]){})) {
    const int num_threads = 6;
    pthread_t threads[num_threads];
    validation_thread_data_t thread_data[num_threads];
    
    // Create validation threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        int ret = pthread_create(&threads[i], NULL, validation_worker, &thread_data[i]);
        tk_assert(ret == 0, "thread creation should succeed");
    }
    
    // Wait for completion
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Check results
    int total_errors = 0;
    int total_allocations = 0;
    
    for (int i = 0; i < num_threads; i++) {
        total_errors += thread_data[i].errors;
        total_allocations += thread_data[i].allocations;
    }
    
    tk_assert(total_errors == 0, "no errors should occur in thread safety test");
    tk_assert(total_allocations > num_threads * 50, "should have reasonable number of successful allocations");
}
