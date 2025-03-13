#include "seff.h"
#include "mem/seff_mem.h"
#include "seff_types.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// 定义一个名为 async_op 的效果，载荷中包含一个 int64_t 类型的 x
DEFINE_EFFECT(async_op, 0, void, { int64_t x; });

// concurrentOperation 用于将当前效果的 x 与后续效果返回的 y 组合起来
// 这里采用 (x + y) 异或 0xABCDEF 的组合方式
static inline int64_t concurrentOperation(int64_t x, int64_t y) {
    return (x + y) ^ 0xABCDEF;
}

// 递归处理协程中产生的 async_op 效果
static int64_t handleAsyncOpRec(seff_coroutine_t *k) {
    seff_request_t req = seff_resume(k, NULL, HANDLES(async_op));
    switch (req.effect) {
        CASE_EFFECT(req, async_op, {
            return concurrentOperation(payload.x, handleAsyncOpRec(k));
        })
        CASE_RETURN(req, {
            return (int64_t) payload.result;
        })
    }
    return -1;
}

// 用于传递给线程的参数
typedef struct thread_args_t {
    int64_t iterations; // 每个协程中调用 async_op 的次数
    int thread_id;      // 当前线程的标识
} thread_args_t;

// 协程函数：在一个循环中调用 async_op 效果
static void* async_loop(void* arg) {
    thread_args_t* args = (thread_args_t*) arg;
    for (int64_t i = args->iterations; i > 0; i--) {
        PERFORM(async_op, i);
    }
    // 返回一个初始状态，这里简单返回 thread_id（也可以是其它初始值）
    return (void*)(intptr_t)args->thread_id;
}

// run_async 创建协程，运行 async_loop 并处理效果，返回最终结果
static int64_t run_async(int64_t iterations, int thread_id) {
    thread_args_t args = {
        .iterations = iterations,
        .thread_id = thread_id
    };
    seff_coroutine_t *k = seff_coroutine_new(async_loop, &args);
    int64_t result = handleAsyncOpRec(k);
    seff_coroutine_delete(k);
    return result;
}

// 每个线程执行的函数：重复调用 run_async 多次并累积结果
void* thread_func(void* arg) {
    thread_args_t* args = (thread_args_t*) arg;
    int64_t local_result = 0;
    // 每个线程重复执行 1000 次
    for (int i = 0; i < 1000; i++) {
        local_result += run_async(args->iterations, args->thread_id);
    }
    return (void*)(intptr_t)local_result;
}

int main(int argc, char** argv) {
    // 默认使用 4 个线程，每个协程进行 10000 次 async_op 调用
    int num_threads = (argc < 2) ? 4 : atoi(argv[1]);
    int64_t iterations = (argc < 3) ? 10000 : atoll(argv[2]);

    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
    thread_args_t* targs = malloc(num_threads * sizeof(thread_args_t));

    // 创建多个线程
    for (int i = 0; i < num_threads; i++) {
        targs[i].iterations = iterations;
        targs[i].thread_id = i + 1;
        pthread_create(&threads[i], NULL, thread_func, &targs[i]);
    }

    int64_t final_result = 0;
    // 聚合所有线程返回的结果
    for (int i = 0; i < num_threads; i++) {
        void* res;
        pthread_join(threads[i], &res);
        final_result += (int64_t)(intptr_t)res;
    }

    free(threads);
    free(targs);

    // 调整输出缓冲区提高性能
    char buffer[8192];
    setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));
    printf("Final aggregated result: %ld\n", final_result);
    return 0;
}

