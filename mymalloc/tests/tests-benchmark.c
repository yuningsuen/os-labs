// GLibC-style malloc benchmark tests
// 模仿 GLibC malloc 的性能基准测试

#include <testkit.h>
#include <pthread.h>
#include <mymalloc.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 获取当前时间（微秒）
static long long get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

// 生成伪随机数（简单的线性同余生成器）
static unsigned int simple_rand(unsigned int *seed) {
    *seed = *seed * 1103515245 + 12345;
    return *seed;
}

// ==================== 基础性能测试 ====================

// 测试1: 顺序分配和释放（模拟典型程序行为）
SystemTest(benchmark_sequential_alloc_free, ((const char *[]){})) {
    const int iterations = 10000;
    const size_t base_size = 64;
    
    long long start_time = get_time_us();
    
    for (int i = 0; i < iterations; i++) {
        void *ptr = mymalloc(base_size);
        if (ptr) {
            // 模拟内存使用
            *((char*)ptr) = (char)(i & 0xFF);
            myfree(ptr);
        }
    }
    
    long long end_time = get_time_us();
    long long duration = end_time - start_time;
    
    // 性能检查：应该在合理时间内完成
    tk_assert(duration < 1000000, "sequential alloc/free should complete within 1 second");
    
    // 打印性能信息（如果启用详细模式）
    #ifndef FREESTANDING
    if (getenv("TK_VERBOSE")) {
        printf("Sequential %d alloc/free: %lld us (%.2f ops/sec)\n", 
               iterations, duration, (double)iterations * 1000000.0 / duration);
    }
    #endif
}

// 测试2: 批量分配后批量释放（模拟数组/缓冲区分配）
SystemTest(benchmark_batch_alloc_free, ((const char *[]){})) {
    const int batch_size = 1000;
    void *ptrs[batch_size];
    
    long long start_time = get_time_us();
    
    // 批量分配
    for (int i = 0; i < batch_size; i++) {
        ptrs[i] = mymalloc(128 + (i % 512)); // 128-639 字节
        if (ptrs[i]) {
            memset(ptrs[i], i & 0xFF, 128 + (i % 512));
        }
    }
    
    // 批量释放
    for (int i = 0; i < batch_size; i++) {
        if (ptrs[i]) {
            myfree(ptrs[i]);
        }
    }
    
    long long end_time = get_time_us();
    long long duration = end_time - start_time;
    
    tk_assert(duration < 500000, "batch alloc/free should complete within 0.5 seconds");
    
    #ifndef FREESTANDING
    if (getenv("TK_VERBOSE")) {
        printf("Batch %d alloc+free: %lld us (%.2f ops/sec)\n", 
               batch_size * 2, duration, (double)batch_size * 2 * 1000000.0 / duration);
    }
    #endif
}

// 测试3: 随机大小分配（模拟真实程序的分配模式）
SystemTest(benchmark_random_sizes, ((const char *[]){})) {
    const int iterations = 5000;
    void *ptrs[100];
    unsigned int seed = 12345;
    
    // 初始化指针数组
    for (int i = 0; i < 100; i++) {
        ptrs[i] = NULL;
    }
    
    long long start_time = get_time_us();
    
    for (int i = 0; i < iterations; i++) {
        int idx = simple_rand(&seed) % 100;
        
        // 如果这个位置有指针，先释放
        if (ptrs[idx]) {
            myfree(ptrs[idx]);
            ptrs[idx] = NULL;
        }
        
        // 随机大小分配（8字节到2KB）
        size_t size = 8 + (simple_rand(&seed) % 2040);
        ptrs[idx] = mymalloc(size);
        
        if (ptrs[idx]) {
            // 写入一些数据
            *((char*)ptrs[idx]) = (char)(i & 0xFF);
            if (size > 1) {
                *((char*)ptrs[idx] + size - 1) = (char)((i >> 8) & 0xFF);
            }
        }
    }
    
    // 清理剩余的分配
    for (int i = 0; i < 100; i++) {
        if (ptrs[i]) {
            myfree(ptrs[i]);
        }
    }
    
    long long end_time = get_time_us();
    long long duration = end_time - start_time;
    
    tk_assert(duration < 2000000, "random size allocation should complete within 2 seconds");
    
    #ifndef FREESTANDING
    if (getenv("TK_VERBOSE")) {
        printf("Random sizes %d ops: %lld us (%.2f ops/sec)\n", 
               iterations, duration, (double)iterations * 1000000.0 / duration);
    }
    #endif
}

// ==================== 大小分布测试 ====================

// 测试4: 小对象分配（<= 64字节，模拟字符串、小结构体）
SystemTest(benchmark_small_objects, ((const char *[]){})) {
    const int iterations = 20000;
    
    long long start_time = get_time_us();
    
    for (int i = 0; i < iterations; i++) {
        size_t size = 8 + (i % 57); // 8-64字节
        void *ptr = mymalloc(size);
        if (ptr) {
            memset(ptr, i & 0xFF, size);
            myfree(ptr);
        }
    }
    
    long long end_time = get_time_us();
    long long duration = end_time - start_time;
    
    tk_assert(duration < 1500000, "small object allocation should be fast");
    
    #ifndef FREESTANDING
    if (getenv("TK_VERBOSE")) {
        printf("Small objects %d ops: %lld us (%.2f ops/sec)\n", 
               iterations, duration, (double)iterations * 1000000.0 / duration);
    }
    #endif
}

// 测试5: 中等对象分配（64-1024字节，模拟缓冲区、数组）
SystemTest(benchmark_medium_objects, ((const char *[]){})) {
    const int iterations = 5000;
    
    long long start_time = get_time_us();
    
    for (int i = 0; i < iterations; i++) {
        size_t size = 64 + (i % 961); // 64-1024字节
        void *ptr = mymalloc(size);
        if (ptr) {
            // 写入首尾数据验证
            *((int*)ptr) = i;
            *((int*)((char*)ptr + size - sizeof(int))) = ~i;
            myfree(ptr);
        }
    }
    
    long long end_time = get_time_us();
    long long duration = end_time - start_time;
    
    tk_assert(duration < 1000000, "medium object allocation should be efficient");
    
    #ifndef FREESTANDING
    if (getenv("TK_VERBOSE")) {
        printf("Medium objects %d ops: %lld us (%.2f ops/sec)\n", 
               iterations, duration, (double)iterations * 1000000.0 / duration);
    }
    #endif
}

// 测试6: 大对象分配（>1KB，模拟大缓冲区、图像数据）
SystemTest(benchmark_large_objects, ((const char *[]){})) {
    const int iterations = 500;
    
    long long start_time = get_time_us();
    
    for (int i = 0; i < iterations; i++) {
        size_t size = 1024 + (i % 2048); // 1KB-3KB
        void *ptr = mymalloc(size);
        if (ptr) {
            // 只写入关键位置以节省时间
            *((int*)ptr) = i;
            *((int*)((char*)ptr + size/2)) = i + 1000;
            *((int*)((char*)ptr + size - sizeof(int))) = i + 2000;
            myfree(ptr);
        }
    }
    
    long long end_time = get_time_us();
    long long duration = end_time - start_time;
    
    tk_assert(duration < 800000, "large object allocation should be reasonable");
    
    #ifndef FREESTANDING
    if (getenv("TK_VERBOSE")) {
        printf("Large objects %d ops: %lld us (%.2f ops/sec)\n", 
               iterations, duration, (double)iterations * 1000000.0 / duration);
    }
    #endif
}

// ==================== 碎片化和内存使用测试 ====================

// 测试7: 内存碎片化测试（交替分配大小对象）
SystemTest(benchmark_fragmentation, ((const char *[]){})) {
    const int pairs = 50;  // 进一步减少以避免栈溢出
    void *small_ptrs[50];  // 使用固定大小避免 VLA 问题
    void *large_ptrs[50];
    
    long long start_time = get_time_us();
    
    // 第一阶段：交替分配大小对象
    for (int i = 0; i < pairs; i++) {
        small_ptrs[i] = mymalloc(32);  // 小对象
        large_ptrs[i] = mymalloc(128); // 中等对象（降低内存压力）
        
        if (small_ptrs[i]) *((int*)small_ptrs[i]) = i;
        if (large_ptrs[i]) *((int*)large_ptrs[i]) = i + 10000;
    }
    
    // 第二阶段：释放所有小对象（创建碎片）
    for (int i = 0; i < pairs; i++) {
        if (small_ptrs[i]) {
            myfree(small_ptrs[i]);
            small_ptrs[i] = NULL;
        }
    }
    
    // 第三阶段：尝试分配中等大小对象（测试碎片整理）
    int successful_allocs = 0;
    for (int i = 0; i < pairs / 2; i++) {
        void *ptr = mymalloc(64); // 中等大小
        if (ptr) {
            successful_allocs++;
            *((int*)ptr) = i + 20000;
            myfree(ptr);
        }
    }
    
    // 清理大对象
    for (int i = 0; i < pairs; i++) {
        if (large_ptrs[i]) {
            myfree(large_ptrs[i]);
        }
    }
    
    long long end_time = get_time_us();
    long long duration = end_time - start_time;
    
    // 检查碎片整理效果
    tk_assert(successful_allocs > pairs / 4, "should handle fragmentation reasonably");
    tk_assert(duration < 1500000, "fragmentation test should complete in reasonable time");
    
    #ifndef FREESTANDING
    if (getenv("TK_VERBOSE")) {
        printf("Fragmentation test: %lld us, %d/%d medium allocs successful (%.1f%%)\n", 
               duration, successful_allocs, pairs/2, 
               (double)successful_allocs * 100.0 / (pairs/2));
    }
    #endif
}

// ==================== 并发性能测试 ====================

typedef struct {
    int thread_id;
    int operations;
    long long duration;
    int successful_ops;
} benchmark_thread_data_t;

void *benchmark_concurrent_worker(void *arg) {
    benchmark_thread_data_t *data = (benchmark_thread_data_t *)arg;
    void *ptrs[50];
    unsigned int seed = data->thread_id * 12345;
    
    // 初始化
    for (int i = 0; i < 50; i++) {
        ptrs[i] = NULL;
    }
    
    long long start_time = get_time_us();
    
    for (int i = 0; i < data->operations; i++) {
        int idx = simple_rand(&seed) % 50;
        
        if (ptrs[idx]) {
            // 释放现有内存
            myfree(ptrs[idx]);
            ptrs[idx] = NULL;
        }
        
        // 分配新内存
        size_t size = 16 + (simple_rand(&seed) % 500); // 16-515字节
        ptrs[idx] = mymalloc(size);
        
        if (ptrs[idx]) {
            data->successful_ops++;
            // 写入测试数据
            *((int*)ptrs[idx]) = data->thread_id * 1000 + i;
        }
        
        // 偶尔清理一些内存
        if ((i % 20) == 0) {
            for (int j = 0; j < 5; j++) {
                if (ptrs[j]) {
                    myfree(ptrs[j]);
                    ptrs[j] = NULL;
                }
            }
        }
    }
    
    // 清理剩余内存
    for (int i = 0; i < 50; i++) {
        if (ptrs[i]) {
            myfree(ptrs[i]);
        }
    }
    
    long long end_time = get_time_us();
    data->duration = end_time - start_time;
    
    return NULL;
}

// 测试8: 多线程并发性能
SystemTest(benchmark_concurrent_performance, ((const char *[]){})) {
    const int num_threads = 4;
    const int ops_per_thread = 2000;
    
    pthread_t threads[num_threads];
    benchmark_thread_data_t thread_data[num_threads];
    
    long long total_start = get_time_us();
    
    // 创建线程
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].operations = ops_per_thread;
        thread_data[i].duration = 0;
        thread_data[i].successful_ops = 0;
        
        int ret = pthread_create(&threads[i], NULL, benchmark_concurrent_worker, &thread_data[i]);
        tk_assert(ret == 0, "thread creation should succeed");
    }
    
    // 等待线程完成
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    long long total_end = get_time_us();
    long long total_duration = total_end - total_start;
    
    // 计算统计信息
    int total_successful = 0;
    long long avg_thread_duration = 0;
    
    for (int i = 0; i < num_threads; i++) {
        total_successful += thread_data[i].successful_ops;
        avg_thread_duration += thread_data[i].duration;
    }
    avg_thread_duration /= num_threads;
    
    // 性能检查
    tk_assert(total_duration < 5000000, "concurrent test should complete within 5 seconds");
    tk_assert(total_successful > num_threads * ops_per_thread * 0.7, 
              "most concurrent operations should succeed");
    
    #ifndef FREESTANDING
    if (getenv("TK_VERBOSE")) {
        printf("Concurrent %d threads: total=%lld us, avg_thread=%lld us, successful=%d/%d (%.1f%%)\n", 
               num_threads, total_duration, avg_thread_duration, 
               total_successful, num_threads * ops_per_thread,
               (double)total_successful * 100.0 / (num_threads * ops_per_thread));
        
        double total_ops_per_sec = (double)total_successful * 1000000.0 / total_duration;
        printf("Throughput: %.2f ops/sec total, %.2f ops/sec per thread\n",
               total_ops_per_sec, total_ops_per_sec / num_threads);
    }
    #endif
}

// 测试9: 内存压力测试（接近内存限制）
SystemTest(benchmark_memory_pressure, ((const char *[]){})) {
    const int max_ptrs = 50;  // 减少以避免内存压力过大
    void *ptrs[50];
    int allocated_count = 0;
    
    long long start_time = get_time_us();
    
    // 持续分配直到接近内存限制
    for (int i = 0; i < max_ptrs; i++) {
        ptrs[i] = mymalloc(1024); // 1KB blocks
        if (ptrs[i]) {
            allocated_count++;
            *((int*)ptrs[i]) = i;
        } else {
            ptrs[i] = NULL;
            break; // 内存不足，停止分配
        }
    }
    
    // 释放一半内存
    for (int i = 0; i < allocated_count; i += 2) {
        if (ptrs[i]) {
            myfree(ptrs[i]);
            ptrs[i] = NULL;
        }
    }
    
    // 尝试重新分配
    int realloc_count = 0;
    for (int i = 0; i < allocated_count; i += 2) {
        ptrs[i] = mymalloc(512); // 更小的块
        if (ptrs[i]) {
            realloc_count++;
            *((int*)ptrs[i]) = i + 1000;
        }
    }
    
    // 清理所有内存
    for (int i = 0; i < max_ptrs; i++) {
        if (ptrs[i]) {
            myfree(ptrs[i]);
        }
    }
    
    long long end_time = get_time_us();
    long long duration = end_time - start_time;
    
    tk_assert(allocated_count > 10, "should be able to allocate some memory");
    tk_assert(realloc_count > 0, "should be able to reallocate after freeing");
    tk_assert(duration < 2000000, "memory pressure test should complete within 2 seconds");
    
    #ifndef FREESTANDING
    if (getenv("TK_VERBOSE")) {
        printf("Memory pressure: %lld us, allocated=%d, reallocated=%d\n", 
               duration, allocated_count, realloc_count);
    }
    #endif
}

// 测试10: 真实应用模拟（混合工作负载）
SystemTest(benchmark_realistic_workload, ((const char *[]){})) {
    const int iterations = 3000;
    void *long_lived[20];    // 长期存活的对象
    void *temp_ptrs[10];     // 临时对象
    unsigned int seed = 54321;
    
    // 初始化
    for (int i = 0; i < 20; i++) long_lived[i] = NULL;
    for (int i = 0; i < 10; i++) temp_ptrs[i] = NULL;
    
    long long start_time = get_time_us();
    
    // 分配一些长期对象
    for (int i = 0; i < 20; i++) {
        long_lived[i] = mymalloc(256 + (i * 50)); // 256-1206字节
        if (long_lived[i]) {
            memset(long_lived[i], i, 256 + (i * 50));
        }
    }
    
    // 主循环：混合短期和临时分配
    for (int i = 0; i < iterations; i++) {
        // 80%概率：临时小对象
        if ((simple_rand(&seed) % 100) < 80) {
            int idx = simple_rand(&seed) % 10;
            if (temp_ptrs[idx]) myfree(temp_ptrs[idx]);
            
            size_t size = 16 + (simple_rand(&seed) % 200); // 16-215字节
            temp_ptrs[idx] = mymalloc(size);
            if (temp_ptrs[idx]) {
                *((int*)temp_ptrs[idx]) = i;
            }
        }
        // 15%概率：中等大小对象
        else if ((simple_rand(&seed) % 100) < 95) {
            void *ptr = mymalloc(512 + (simple_rand(&seed) % 1024)); // 512-1535字节
            if (ptr) {
                memset(ptr, i & 0xFF, 100); // 只写前100字节
                myfree(ptr);
            }
        }
        // 5%概率：替换长期对象
        else {
            int idx = simple_rand(&seed) % 20;
            if (long_lived[idx]) {
                myfree(long_lived[idx]);
            }
            long_lived[idx] = mymalloc(300 + (simple_rand(&seed) % 500)); // 300-799字节
            if (long_lived[idx]) {
                *((int*)long_lived[idx]) = i + 10000;
            }
        }
    }
    
    // 清理所有内存
    for (int i = 0; i < 20; i++) {
        if (long_lived[i]) myfree(long_lived[i]);
    }
    for (int i = 0; i < 10; i++) {
        if (temp_ptrs[i]) myfree(temp_ptrs[i]);
    }
    
    long long end_time = get_time_us();
    long long duration = end_time - start_time;
    
    tk_assert(duration < 3000000, "realistic workload should complete within 3 seconds");
    
    #ifndef FREESTANDING
    if (getenv("TK_VERBOSE")) {
        printf("Realistic workload %d ops: %lld us (%.2f ops/sec)\n", 
               iterations, duration, (double)iterations * 1000000.0 / duration);
    }
    #endif
}
