#include "seff.h"
#include "mem/seff_mem.h"
#include "seff_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/resource.h>  // 用于 getrusage()

#define SMALL_STACK_SIZE 10  // 每个协程使用较小的栈空间

// 定义一个效果 process_op，用于模拟每个轻量级进程的内存操作，携带一个进程ID
DEFINE_EFFECT(process_op, 0, void, { int64_t pid; });

// 定义一个简单的操作：将当前进程ID与后续返回的结果累加
static inline int64_t processOperation(int64_t pid, int64_t res) {
    return res + pid;
}

// 递归处理 process_op 效果
static int64_t handleProcessOpRec(seff_coroutine_t *k) {
    seff_request_t req = seff_resume(k, NULL, HANDLES(process_op));
    switch (req.effect) {
        CASE_EFFECT(req, process_op, {
            return processOperation(payload.pid, handleProcessOpRec(k));
        })
        CASE_RETURN(req, {
            return (int64_t) payload.result;
        })
    }
    return -1; // 理论上不会执行到这里
}

// 模拟一个轻量级进程（协程）函数：该函数触发 process_op 效果，模拟内核中某些内存操作
static void* process_func(void* arg) {
    int64_t pid = (int64_t)arg;
    // 模拟进程内某个操作：触发 process_op 效果，将进程ID传入
    PERFORM(process_op, pid);
    // 返回初始值 0 作为协程的最终返回值
    return (void*)0;
}

int main(int argc, char** argv) {
    // 默认创建 10000 个轻量级进程（协程），也可通过命令行参数指定
    int64_t num_processes = (argc < 2) ? 10 : atoll(argv[1]);
    int64_t total = 0;

    // 分配存储所有协程指针的数组
    seff_coroutine_t **processes = malloc(num_processes * sizeof(seff_coroutine_t*));
    if (!processes) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    // 利用 seff_coroutine_new_sized 创建大量轻量级进程，每个协程分配较小的栈以降低内存占用
    for (int64_t i = 0; i < num_processes; i++) {
        processes[i] = seff_coroutine_new_sized(process_func, (void*)i, SMALL_STACK_SIZE);
        if (!processes[i]) {
            
            return EXIT_FAILURE;
        }
    }

    // 依次运行每个协程并利用效果处理器处理 process_op 效果
    for (int64_t i = 0; i < num_processes; i++) {
        int64_t res = handleProcessOpRec(processes[i]);
        total += res;
        seff_coroutine_delete(processes[i]);
    }
    free(processes);

    // 获取进程内存使用情况
    struct rusage usage;
    if(getrusage(RUSAGE_SELF, &usage) == 0) {
        // ru_maxrss 单位通常为 KB
        printf("u: %ld KB\n", usage.ru_maxrss);
    } else {
        perror("getrusage");
    }

    
    
    return 0;
}

