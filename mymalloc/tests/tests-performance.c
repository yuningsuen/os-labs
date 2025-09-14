// Performance benchmark tests for mymalloc/myfree

#include <testkit.h>
#include <pthread.h>
#include <mymalloc.h>
#include <time.h>
#include <sys/time.h>

// Get current time in microseconds
static long long get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

// Single-threaded performance test
SystemTest(single_thread_performance, ((const char *[]){})) {
    const int num_operations = 10000;
    void *ptrs[1000];
    
    long long start_time = get_time_us();
    
    // Mixed allocation/deallocation pattern
    for (int i = 0; i < num_operations; i++) {
        int idx = i % 1000;
        
        if (ptrs[idx] != NULL) {
            myfree(ptrs[idx]);
            ptrs[idx] = NULL;
        }
        
        size_t size = 16 + (i % 512);  // 16 to 527 bytes
        ptrs[idx] = mymalloc(size);
        
        if (ptrs[idx]) {
            // Touch the memory to ensure it's valid
            *((char*)ptrs[idx]) = (char)(i & 0xFF);
        }
    }
    
    // Clean up remaining allocations
    for (int i = 0; i < 1000; i++) {
        if (ptrs[i] != NULL) {
            myfree(ptrs[i]);
        }
    }
    
    long long end_time = get_time_us();
    long long duration = end_time - start_time;
    
    // Should complete within reasonable time (adjust as needed)
    tk_assert(duration < 5000000, "single-threaded test should complete within 5 seconds");
    
    // Print performance info if verbose
    // printf("Single-threaded %d operations took %lld microseconds\n", 
    //        num_operations, duration);
}

typedef struct {
    int thread_id;
    int operations;
    long long duration;
} perf_thread_data_t;

void *perf_worker(void *arg) {
    perf_thread_data_t *data = (perf_thread_data_t *)arg;
    void *ptrs[100];
    
    // Initialize pointers
    for (int i = 0; i < 100; i++) {
        ptrs[i] = NULL;
    }
    
    long long start_time = get_time_us();
    
    for (int i = 0; i < data->operations; i++) {
        int idx = i % 100;
        
        // Free existing allocation
        if (ptrs[idx] != NULL) {
            myfree(ptrs[idx]);
        }
        
        // Allocate new memory
        size_t size = 16 + ((data->thread_id * 1000 + i) % 256);
        ptrs[idx] = mymalloc(size);
        
        if (ptrs[idx]) {
            // Touch the memory
            *((char*)ptrs[idx]) = (char)(i & 0xFF);
        }
    }
    
    // Clean up
    for (int i = 0; i < 100; i++) {
        if (ptrs[i] != NULL) {
            myfree(ptrs[i]);
        }
    }
    
    long long end_time = get_time_us();
    data->duration = end_time - start_time;
    
    return NULL;
}

// Multi-threaded performance test
SystemTest(multi_thread_performance, ((const char *[]){})) {
    const int num_threads = 4;
    const int ops_per_thread = 2000;
    
    pthread_t threads[num_threads];
    perf_thread_data_t thread_data[num_threads];
    
    long long total_start = get_time_us();
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].operations = ops_per_thread;
        thread_data[i].duration = 0;
        
        int ret = pthread_create(&threads[i], NULL, perf_worker, &thread_data[i]);
        tk_assert(ret == 0, "thread creation should succeed");
    }
    
    // Wait for threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    long long total_end = get_time_us();
    long long total_duration = total_end - total_start;
    
    // Should complete within reasonable time
    tk_assert(total_duration < 10000000, "multi-threaded test should complete within 10 seconds");
    
    // Calculate average thread duration
    long long avg_thread_duration = 0;
    for (int i = 0; i < num_threads; i++) {
        avg_thread_duration += thread_data[i].duration;
    }
    avg_thread_duration /= num_threads;
    
    // Print performance info if verbose
    // printf("Multi-threaded test: total=%lld us, avg_thread=%lld us\n", 
    //        total_duration, avg_thread_duration);
    
    tk_assert(1, "performance test completed");
}

// Memory fragmentation test
SystemTest(fragmentation_test, ((const char *[]){})) {
    const int num_blocks = 50;
    void *ptrs[num_blocks];
    
    // Allocate many small blocks
    for (int i = 0; i < num_blocks; i++) {
        ptrs[i] = mymalloc(64);
        tk_assert(ptrs[i] != NULL, "small allocation should succeed");
    }
    
    // Free every other block to create fragmentation
    for (int i = 1; i < num_blocks; i += 2) {
        myfree(ptrs[i]);
        ptrs[i] = NULL;
    }
    
    // Try to allocate a larger block
    void *large1 = mymalloc(128);
    void *large2 = mymalloc(256);
    
    // At least one should succeed due to coalescing or finding space
    tk_assert(large1 != NULL || large2 != NULL, 
              "should be able to allocate larger blocks despite fragmentation");
    
    // Clean up
    for (int i = 0; i < num_blocks; i += 2) {
        if (ptrs[i] != NULL) {
            myfree(ptrs[i]);
        }
    }
    
    if (large1) myfree(large1);
    if (large2) myfree(large2);
}

// Test allocation patterns common in real applications
SystemTest(realistic_patterns, ((const char *[]){})) {
    // Pattern 1: Many small allocations (like malloc for strings)
    void *small_ptrs[200];
    for (int i = 0; i < 200; i++) {
        small_ptrs[i] = mymalloc(8 + (i % 32));  // 8-39 bytes
        tk_assert(small_ptrs[i] != NULL, "small allocation should succeed");
    }
    
    // Pattern 2: Few large allocations (like buffers)
    void *large_ptrs[10];
    for (int i = 0; i < 10; i++) {
        large_ptrs[i] = mymalloc(1024 + (i * 512));  // 1KB to 5.5KB
        // Some might fail due to memory constraints, that's okay
    }
    
    // Pattern 3: Mixed free pattern (free some small, some large)
    for (int i = 0; i < 200; i += 3) {
        myfree(small_ptrs[i]);
        small_ptrs[i] = NULL;
    }
    
    for (int i = 0; i < 10; i += 2) {
        if (large_ptrs[i] != NULL) {
            myfree(large_ptrs[i]);
            large_ptrs[i] = NULL;
        }
    }
    
    // Pattern 4: Reallocate in freed spaces
    for (int i = 0; i < 200; i += 3) {
        small_ptrs[i] = mymalloc(16 + (i % 24));
    }
    
    // Clean up everything
    for (int i = 0; i < 200; i++) {
        if (small_ptrs[i] != NULL) {
            myfree(small_ptrs[i]);
        }
    }
    
    for (int i = 0; i < 10; i++) {
        if (large_ptrs[i] != NULL) {
            myfree(large_ptrs[i]);
        }
    }
    
    tk_assert(1, "realistic allocation patterns test completed");
}
