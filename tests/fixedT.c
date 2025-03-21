/*
 *
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
 *
 * 本示例用于测试fixed size stack在接近栈容量上限时的性能，
 * 通过在协程中执行深度递归来填满固定栈空间，
 * 并通过重复多次测试以评估边界条件下的额外开销。
 *
 */

#include "seff.h"
#include "mem/seff_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>



// 递归函数：简单的递归计数，用于消耗栈空间
static int64_t recursive_count(int64_t depth) {
    // 基准情况
    if (depth == 0)
        return 0;
    // 为了防止尾递归优化，加上额外运算
    volatile int dummy = 0;
    dummy++;
    return 1 + recursive_count(depth - 1);
}

// 协程函数：在固定栈上执行深度递归
static void* stack_test_coroutine(void* arg) {
    int64_t depth = *(int64_t*)arg;
    int64_t res = recursive_count(depth);
    // 将结果转换为void*返回
    return (void*)(intptr_t)res;
}

// 运行一次协程测试，传入递归深度和固定栈大小（单位：字节）
static int64_t run_stack_test(int64_t depth, size_t frame_size) {
    // 创建一个使用指定frame_size的协程
    seff_coroutine_t* k = seff_coroutine_new_sized(stack_test_coroutine, &depth, frame_size);
    // 直接恢复协程（本例中不涉及effect，因此直接获取返回结果）
    seff_request_t req = seff_resume_handling_all(k, NULL);
    int64_t res = (int64_t)(intptr_t)req.payload;
    seff_coroutine_delete(k);
    return res;
}

int main(int argc, char** argv) {
    // 命令行参数：
    // argv[1]: 递归深度（建议设置为接近栈容量的帧大小 / 递归每帧占用字节数，比如500）
    // argv[2]: 重复次数
    // argv[3]: 固定栈大小（单位字节），默认为8KB
    int64_t depth = (argc < 2) ? 50 : atoll(argv[1]);
    int64_t repeats = (argc < 3) ? 10 : atoll(argv[2]);
    size_t frame_size = (argc < 4) ? (8 * 1024) : (size_t)atoll(argv[3]);

    // 调整输出缓冲区大小（与原测试保持一致）
    char buffer[8192];
    setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int64_t result = 0;
    for (int64_t i = 0; i < repeats; i++) {
        result += run_stack_test(depth, frame_size);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Result: %" PRId64 "\n", result);
    printf("Elapsed time: %f seconds\n", elapsed);
    return 0;
}

