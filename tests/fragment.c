
#include "seff.h"
#include "mem/seff_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>
#include <pthread.h>


// 定义一个用于表示协程栈空间测量的效果，不携带额外数据
DEFINE_EFFECT(stack_fragment, 0, void, { });

// 全局变量，用于记录每次测量得到的当前栈使用量（单位：字节）
unsigned long current_stack_usage = 0;

// 获取当前栈指针的函数（适用于 x86_64 架构）
static inline void* get_sp() {
    void* sp;
    asm volatile ("mov %%rsp, %0" : "=r"(sp));
    return sp;
}

// 递归函数，不断在栈上分配 1KB 空间以消耗栈内存
// 参数 base 为协程初始时的栈指针（即栈基地址），用于计算当前使用量
void stack_fragment_rec(int depth, int max_depth, void* base) {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));  // 模拟工作负载
    if (depth < max_depth) {
        stack_fragment_rec(depth + 1, max_depth, base);
    } else {
        // 当达到最大深度时，测量当前栈使用情况
        void* sp = get_sp();
        // 假设栈向下增长，则已用栈空间 = base - sp
        current_stack_usage = (uintptr_t)base - (uintptr_t)sp;
        // 触发效果，通知主调度器捕捉该测量点
        PERFORM(stack_fragment, 0);
    }
}

// 协程函数：首先记录当前栈基地址，然后调用递归函数
void* stack_fragment_fn(void* arg) {
    int max_depth = *(int*)arg;
    // 获取当前栈指针作为栈基地址（在协程创建时该值相对固定）
    void* stack_base = get_sp();
    stack_fragment_rec(0, max_depth, stack_base);
    return NULL;
}

int main(int argc, char* argv[]) {
    // 设定最大递归深度，默认 100 次；可通过命令行参数调整
    int max_depth = 100;
    if (argc > 1) {
        max_depth = atoi(argv[1]);
    }
    
    // 创建协程，并传入最大递归深度参数
    seff_coroutine_t* co = seff_coroutine_new(stack_fragment_fn, &max_depth);
    
    // 用于统计栈使用情况的峰值和累计总和（便于计算平均值）
    unsigned long peak_usage = 0;
    unsigned long total_usage = 0;
    int count = 0;
    
    // 效果处理循环：每次捕捉到 stack_fragment 效果时，统计当前栈使用量
    seff_request_t req = seff_resume(co, NULL, HANDLES(stack_fragment));
    while (req.effect != EFF_ID(return)) {
        if (current_stack_usage > peak_usage)
            peak_usage = current_stack_usage;
        total_usage += current_stack_usage;
        count++;
        req = seff_resume(co, NULL, HANDLES(stack_fragment));
    }
    
    unsigned long avg_usage = (count > 0) ? total_usage / count : 0;
    printf("Coroutine Stack Utilization and Fragmentation Test Results:\n");
    printf("Iterations: %d\n", count);
    printf("Peak stack usage: %lu bytes\n", peak_usage);
    printf("Average stack usage: %lu bytes\n", avg_usage);
    
    seff_coroutine_delete(co);
    return 0;
}

