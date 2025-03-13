/*
 *
 * Copyright (c) 2023 Huawei Technologies Co., Ltd.
 *
 * 本示例用于测试效果处理器涉及协程上下文切换时的开销。
 * 协程函数在循环中多次触发自定义的 ctx_switch 效果，
 * 效果处理器则通过递归方式处理每次效果，从而模拟上下文切换。
 *
 */

#include "seff.h"
#include "mem/seff_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>

// 定义一个自定义效果 ctx_switch，无载荷或仅作占位
DEFINE_EFFECT(ctx_switch, 0, void, { int dummy; });

// 一个简单的上下文切换运算，用于组合效果处理的返回值
// 这里采用简单的加法运算作为示例
static inline int64_t contextSwitchOp(int64_t x, int64_t y) {
    return x + y;
}

// 递归处理 ctx_switch 效果：每次遇到效果时，将 1 与后续处理结果做简单组合
static int64_t handleCtxSwitchRec(seff_coroutine_t *k) {
    seff_request_t req = seff_resume(k, NULL, HANDLES(ctx_switch));
    switch (req.effect) {
        CASE_EFFECT(req, ctx_switch, {
            return contextSwitchOp(1, handleCtxSwitchRec(k));
        })
        CASE_RETURN(req, {
            return (int64_t)(intptr_t) req.payload;
        })
    }
    return -1;
}

// 协程函数：在循环中触发多次 ctx_switch 效果
static void* ctx_switch_loop(void* arg) {
    int64_t iterations = *(int64_t*)arg;
    for (int64_t i = 0; i < iterations; i++) {
        PERFORM(ctx_switch, 0);
    }
    return (void*)(intptr_t) 0;
}

// run_context_switch 创建协程并运行 ctx_switch_loop，通过效果处理器处理所有效果
static int64_t run_context_switch(int64_t iterations) {
    seff_coroutine_t* k = seff_coroutine_new(ctx_switch_loop, &iterations);
    int64_t result = handleCtxSwitchRec(k);
    seff_coroutine_delete(k);
    return result;
}

int main(int argc, char** argv) {
    // 第一个命令行参数控制上下文切换（即效果触发）的次数
    int64_t iterations = (argc < 2) ? 1000000 : atoll(argv[1]);

    // 调整输出缓冲区
    char buffer[8192];
    setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    int64_t res = run_context_switch(iterations);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Result: %" PRId64 "\n", res);
    printf("Elapsed time: %f seconds\n", elapsed);
    return 0;
}

