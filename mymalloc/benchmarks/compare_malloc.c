// malloc 性能对比工具
// 比较 mymalloc 和系统 malloc 的性能

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#define MYMALLOC_TEST
#ifdef MYMALLOC_TEST
#include "mymalloc.h"
#define test_malloc mymalloc
#define test_free myfree
#else
#define test_malloc malloc
#define test_free free
#endif

// 获取时间（微秒）
static long long get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

// 简单随机数生成器
static unsigned int simple_rand(unsigned int *seed) {
    *seed = *seed * 1103515245 + 12345;
    return *seed;
}

// 基准测试1: 顺序分配释放
void benchmark_sequential(int iterations) {
    printf("Sequential Allocation Test (%d iterations):\n", iterations);
    
    long long start = get_time_us();
    
    for (int i = 0; i < iterations; i++) {
        void *ptr = test_malloc(64);
        if (ptr) {
            *((char*)ptr) = (char)(i & 0xFF);
            test_free(ptr);
        }
    }
    
    long long end = get_time_us();
    long long duration = end - start;
    
    printf("  Time: %lld us\n", duration);
    printf("  Rate: %.2f ops/sec\n", (double)iterations * 1000000.0 / duration);
    printf("  Avg:  %.2f us/op\n", (double)duration / iterations);
}

// 基准测试2: 批量分配释放
void benchmark_batch(int batch_size) {
    printf("\nBatch Allocation Test (%d objects):\n", batch_size);
    
    void **ptrs = malloc(batch_size * sizeof(void*));
    if (!ptrs) {
        printf("  Failed to allocate pointer array\n");
        return;
    }
    
    long long start = get_time_us();
    
    // 批量分配
    for (int i = 0; i < batch_size; i++) {
        ptrs[i] = test_malloc(128 + (i % 384)); // 128-511 bytes
        if (ptrs[i]) {
            *((int*)ptrs[i]) = i;
        }
    }
    
    // 批量释放
    for (int i = 0; i < batch_size; i++) {
        if (ptrs[i]) {
            test_free(ptrs[i]);
        }
    }
    
    long long end = get_time_us();
    long long duration = end - start;
    
    printf("  Time: %lld us\n", duration);
    printf("  Rate: %.2f ops/sec\n", (double)(batch_size * 2) * 1000000.0 / duration);
    printf("  Avg:  %.2f us/op\n", (double)duration / (batch_size * 2));
    
    free(ptrs);
}

// 基准测试3: 随机大小分配
void benchmark_random_sizes(int iterations) {
    printf("\nRandom Size Allocation Test (%d iterations):\n", iterations);
    
    void *ptrs[100];
    unsigned int seed = 12345;
    
    // 初始化
    for (int i = 0; i < 100; i++) {
        ptrs[i] = NULL;
    }
    
    long long start = get_time_us();
    
    for (int i = 0; i < iterations; i++) {
        int idx = simple_rand(&seed) % 100;
        
        if (ptrs[idx]) {
            test_free(ptrs[idx]);
        }
        
        size_t size = 8 + (simple_rand(&seed) % 1016); // 8-1023 bytes
        ptrs[idx] = test_malloc(size);
        
        if (ptrs[idx]) {
            *((char*)ptrs[idx]) = (char)(i & 0xFF);
        }
    }
    
    // 清理
    for (int i = 0; i < 100; i++) {
        if (ptrs[i]) {
            test_free(ptrs[i]);
        }
    }
    
    long long end = get_time_us();
    long long duration = end - start;
    
    printf("  Time: %lld us\n", duration);
    printf("  Rate: %.2f ops/sec\n", (double)iterations * 1000000.0 / duration);
    printf("  Avg:  %.2f us/op\n", (double)duration / iterations);
}

// 基准测试4: 碎片化测试
void benchmark_fragmentation() {
    printf("\nFragmentation Test:\n");
    
    const int pairs = 500;
    void **small_ptrs = malloc(pairs * sizeof(void*));
    void **large_ptrs = malloc(pairs * sizeof(void*));
    
    if (!small_ptrs || !large_ptrs) {
        printf("  Failed to allocate arrays\n");
        return;
    }
    
    long long start = get_time_us();
    
    // 交替分配大小对象
    for (int i = 0; i < pairs; i++) {
        small_ptrs[i] = test_malloc(32);
        large_ptrs[i] = test_malloc(256);
    }
    
    // 释放小对象（创建碎片）
    for (int i = 0; i < pairs; i++) {
        if (small_ptrs[i]) {
            test_free(small_ptrs[i]);
            small_ptrs[i] = NULL;
        }
    }
    
    // 尝试分配中等对象
    int successful = 0;
    for (int i = 0; i < pairs / 2; i++) {
        void *ptr = test_malloc(64);
        if (ptr) {
            successful++;
            test_free(ptr);
        }
    }
    
    // 清理大对象
    for (int i = 0; i < pairs; i++) {
        if (large_ptrs[i]) {
            test_free(large_ptrs[i]);
        }
    }
    
    long long end = get_time_us();
    long long duration = end - start;
    
    printf("  Time: %lld us\n", duration);
    printf("  Success rate: %.1f%% (%d/%d)\n", 
           (double)successful * 100.0 / (pairs/2), successful, pairs/2);
    
    free(small_ptrs);
    free(large_ptrs);
}

int main() {
    #ifdef MYMALLOC_TEST
    printf("========================================\n");
    printf("   mymalloc Performance Benchmark\n");
    printf("========================================\n\n");
    #else
    printf("========================================\n");
    printf("   System malloc Performance Benchmark\n");
    printf("========================================\n\n");
    #endif
    
    // 运行各种基准测试
    benchmark_sequential(10000);
    benchmark_batch(1000);
    benchmark_random_sizes(5000);
    benchmark_fragmentation();
    
    printf("\n========================================\n");
    printf("Benchmark completed.\n");
    
    #ifdef MYMALLOC_TEST
    printf("\nTo compare with system malloc:\n");
    printf("  gcc -O2 -o compare_system compare_malloc.c\n");
    printf("  ./compare_system\n");
    #endif
    
    return 0;
}
