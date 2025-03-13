

#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <malloc.h>

#include <inttypes.h>

#include "scheff.h"
#include "seff.c"
#include "seff.h"
#include "mem/seff_mem.h"
#include "mem/seff_mem_segmented.c"
#include "seff_types.h"
#include <stdlib.h>

// 设置线程局部的当前协程指针
// （确保与 libseff 的实现保持一致）
extern __thread seff_coroutine_t * _seff_current_coroutine;


int main(void) {
    // 创建一个 dummy 协程结构体，用于模拟当前正在运行的协程
    seff_coroutine_t dummy;
    _seff_current_coroutine = &dummy;

    // 初始化初始栈帧（segmented stack 的底层分配）
    char *rsp;
    size_t initial_frame_size = DEFAULT_DEFAULT_FRAME_SIZE;  // 默认帧大小，通常在 seff_mem_common.h 中定义
    dummy.frame_ptr = init_stack_frame(initial_frame_size, &rsp);

    // 打印初始内存分配情况
    printf("=== 内存状态：分配前 ===\n");
    malloc_stats();

    // 模拟多次栈帧分配
    int iterations = 10000;      // 分配次数
    size_t alloc_size = 256;     // 每次需要分配的额外栈空间大小（单位字节）
    size_t frame_size = alloc_size; // 传入 seff_mem_allocate_frame 的初始帧大小

    for (int i = 0; i < iterations; i++) {
        // 模拟将当前栈内容拷贝到新分配的栈帧中
        // seff_mem_allocate_frame 会根据需求分配新 segment（如果必要的话）
        void *new_rsp = seff_mem_allocate_frame(&frame_size, rsp, alloc_size);
        rsp = new_rsp;
    }

    printf("=== 内存状态：分配 %d 次每次 %zu 字节的栈帧后 ===\n", iterations, alloc_size);
    malloc_stats();

    // 模拟释放所有栈帧（类似递归函数返回）
    for (int i = 0; i < iterations; i++) {
        rsp = seff_mem_release_frame();
    }

    printf("=== 内存状态：释放所有栈帧后 ===\n");
    malloc_stats();

    return 0;
}

