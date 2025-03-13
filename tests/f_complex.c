/*
 *
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
 *
 * 本程序用于测试在效果处理器中涉及额外计算、资源竞争（同步）以及状态保存时的上下文切换开销。
 * 每次上下文切换不仅仅是简单的yield，还会执行额外的计算和锁操作，以模拟更复杂的工作负载。
 *
 */

#include "seff.h"
#include "mem/seff_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>
#include <pthread.h>

// 定义一个名为 complex_yield 的效果，不携带额外数据
DEFINE_EFFECT(complex_yield, 0, void, { });

// 全局互斥锁，用于模拟资源竞争
static pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;
// 全局状态变量，用于模拟状态保存
static volatile int dummy_global = 0;

// 模拟额外的计算工作：执行一段简单循环运算，并更新全局状态
static inline void extra_work(void) {
    int sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += i;
    }
    dummy_global += sum;
}

// 协程函数：在每次迭代中执行简单计算后触发 complex_yield 效果
static void* complex_yield_coroutine(void* arg) {
    int64_t iterations = *(int64_t*)arg;
    for (int64_t i = 0; i < iterations; i++) {
        // 模拟协程内部的计算
        volatile int dummy = i * 2;
        PERFORM(complex_yield, 0);
    }
    return (void*)(intptr_t)iterations;
}

// 效果处理循环：每次遇到 complex_yield 效果时，先进行额外工作，再恢复协程
static int64_t handle_complex_yield_loop(seff_coroutine_t* k) {
    seff_request_t req = seff_resume(k, NULL, HANDLES(complex_yield));
    while (req.effect != EFF_ID(return)) {
        switch (req.effect) {
            CASE_EFFECT(req, complex_yield, {
                // 模拟同步操作：加锁-执行额外计算-解锁
                pthread_mutex_lock(&sync_mutex);
                extra_work();
                pthread_mutex_unlock(&sync_mutex);
                // 恢复协程
                req = seff_resume(k, NULL, HANDLES(complex_yield));
            })
        }
    }
    return (int64_t)(intptr_t)req.payload;
}

int main(int argc, char** argv) {
    // 命令行参数：上下文切换（yield）次数，默认1,000,000次
    int64_t iterations = (argc < 2) ? 1000000 : atoll(argv[1]);

    // 设置输出缓冲区（与其他测试保持一致）
    char buffer[8192];
    setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

    // 记录开始时间
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // 创建并初始化协程
    seff_coroutine_t* k = seff_coroutine_new(complex_yield_coroutine, &iterations);
    int64_t res = handle_complex_yield_loop(k);
    seff_coroutine_delete(k);

    // 记录结束时间
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Complex yield iterations: %" PRId64 "\n", res);
    printf("Elapsed time: %f seconds\n", elapsed);
    printf("Dummy global: %d\n", dummy_global);
    return 0;
}

