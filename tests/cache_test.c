/*
 *
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
 *
 * 本程序用于测试固定大小栈的缓存命中率，
 * 通过使用 init_stack_frame() 分配固定大小栈，
 * 然后分别以顺序和随机方式访问栈内存，
 * 并测量两种模式下的执行时间差异，
 * 从而间接评估缓存局部性对性能的影响。
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>

// 引入你提供的 seff_mem.h，确保其中包含 init_stack_frame 的声明
#include "mem/seff_mem.h"

// 顺序访问函数：以固定步长遍历内存
static volatile uint64_t sequential_access(char *stack, size_t size, size_t step) {
    volatile uint64_t sum = 0;
    for (size_t i = 0; i < size; i += step) {
        sum += stack[i];
    }
    return sum;
}

// 随机访问函数：在内存中随机读取数据，模拟较低缓存命中率的情况
static volatile uint64_t random_access(char *stack, size_t size, size_t iterations) {
    volatile uint64_t sum = 0;
    for (size_t i = 0; i < iterations; i++) {
        size_t index = rand() % size;
        sum += stack[index];
    }
    return sum;
}

int main(int argc, char **argv) {
    // 命令行参数：
    // argv[1]: 固定栈大小（字节），默认为 8192
    // argv[2]: 顺序访问步长，默认为 64（通常与 CPU cache line 大小相关）
    // argv[3]: 随机访问迭代次数，默认为 1000000
    size_t frame_size = (argc > 1) ? atoll(argv[1]) : 8192;
    size_t seq_step = (argc > 2) ? atoll(argv[2]) : 64;
    size_t rand_iterations = (argc > 3) ? atoll(argv[3]) : 1000000;
    
    // 使用 init_stack_frame 分配固定大小栈空间
    char *rsp;
    seff_frame_ptr_t stack = init_stack_frame(frame_size, &rsp);
    if (!stack) {
        fprintf(stderr, "Failed to allocate stack frame\n");
        exit(EXIT_FAILURE);
    }
    
    // 用一个已知的模式初始化内存，确保所有页面被实际分配并加载到内存中
    char *buf = (char *)stack;
    for (size_t i = 0; i < frame_size; i++) {
        buf[i] = (char)(i % 256);
    }
    
    struct timespec start, end;
    double elapsed_seq, elapsed_rand;
    volatile uint64_t seq_sum, rand_sum;
    
    // 测试顺序访问
    clock_gettime(CLOCK_MONOTONIC, &start);
    seq_sum = sequential_access(buf, frame_size, seq_step);
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed_seq = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // 测试随机访问
    clock_gettime(CLOCK_MONOTONIC, &start);
    rand_sum = random_access(buf, frame_size, rand_iterations);
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed_rand = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Fixed stack size: %zu bytes\n", frame_size);
    printf("Sequential access (step %zu) time: %f seconds, sum: %" PRIu64 "\n", seq_step, elapsed_seq, seq_sum);
    printf("Random access (%zu iterations) time: %f seconds, sum: %" PRIu64 "\n", rand_iterations, elapsed_rand, rand_sum);
    printf("libseff using fixed-size stacks\n");
    printf("Initial frame size: %zu\n", frame_size);
    
    free(stack);
    return 0;
}

