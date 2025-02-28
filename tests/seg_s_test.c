

#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <inttypes.h>

#include "scheff.h"
#include "seff.c"
#include "seff.h"
#include "mem/seff_mem.h"
#include "seff_types.h"
#include <stdlib.h>

// 定义测试参数
#define NUM_COROUTINES         100
#define ITERATIONS_PER_COROUTINE 100

// 递归函数，用于模拟深度调用，迫使分段栈扩展
void recursive_work(int depth) {
    if (depth == 0) return;
    volatile int dummy = depth;  // 防止被优化掉
    (void)dummy;
    recursive_work(depth - 1);
}

// 协程入口函数
// 每次被恢复时执行一次“工作”：调用递归函数模拟栈扩展，然后增加计数后主动让出（yield）。
void *coroutine_func(void *arg) {
    // 每个协程传入一个指向 int 的指针，用于记录已完成的迭代次数
    int *p_iter = (int *)arg;
    if (*p_iter < ITERATIONS_PER_COROUTINE) {
        // 模拟深度工作，测试扩展性
        recursive_work(20);
        (*p_iter)++;  // 完成一次迭代

        // 主动让出控制权，模拟上下文切换
        // 这里调用 seff_exit，使当前协程挂起，等待下次恢复。
        // 注意：实际使用时应使用库中提供的 yield 或退出接口，并传入合适的 effect ID（本例用 0 表示 yield 效果）。
        seff_exit(seff_current_coroutine(), 0, NULL);
    }
    // 当迭代次数达到 ITERATIONS_PER_COROUTINE 后，协程函数返回，表示该协程执行完毕。
    return NULL;
}

int main(void) {
    int i;
    // 为每个协程准备一个迭代计数器（初值 0）
    int *iters[NUM_COROUTINES];
    // 保存协程对象的数组
    seff_coroutine_t *coroutines[NUM_COROUTINES];

    // 创建 NUM_COROUTINES 个协程
    for (i = 0; i < NUM_COROUTINES; i++) {
        iters[i] = malloc(sizeof(int));
        if (!iters[i]) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        *iters[i] = 0;
        // 注意：协程入口函数类型必须是 void *(*)(void *), 本例中 coroutine_func 符合要求
        coroutines[i] = seff_coroutine_new(coroutine_func, iters[i]);
        if (!coroutines[i]) {
            fprintf(stderr, "无法创建协程 %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    int total_context_switches = 0;
    int finished_count = 0;

    // 主调度循环：轮询恢复各协程，直到所有协程都完成（状态 FINISHED）
    while (finished_count < NUM_COROUTINES) {
        finished_count = 0;
        for (i = 0; i < NUM_COROUTINES; i++) {
            // 假设每个协程对象的 state 成员能反映是否 FINISHED（
            if (coroutines[i]->state != FINISHED) {
                seff_resume_handling_all(coroutines[i], NULL);
                total_context_switches++;
            } else {
                finished_count++;
            }
        }
    }

    printf("测试完成：总共上下文切换次数 = %d\n", total_context_switches);

    // 清理：删除所有协程对象，并释放各协程的迭代计数器
    for (i = 0; i < NUM_COROUTINES; i++) {
        seff_coroutine_delete(coroutines[i]);
        free(iters[i]);
    }
    return 0;
}

