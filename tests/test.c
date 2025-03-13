
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

void *dummy(void *arg) {
    return NULL;
}


int main(int argc, char **argv) {
    // 创建一个新的协程，使用默认的帧大小
    seff_coroutine_t *co = seff_coroutine_new_sized(dummy, NULL, 1000000);

    if (!co) {
        fprintf(stderr, "无法创建协程\n");
        exit(EXIT_FAILURE);
    }

    // 进行栈操作测试：连续 push 操作
    int num_pushes = 100000; // 例如压入 10000 个帧
    for (int i = 0; i < num_pushes; i++) {
        // frame_push 函数将一个元素压入协程的栈中
        // 这里把当前循环计数转换为指针作为测试数据
        frame_push(&co->resume_point, (void*)(long)i);
    }

    printf("成功压入 %d 个帧\n", num_pushes);

    // 这里可以加入更多的栈操作，比如 pop（如果有相应的函数）等

    // 删除协程，释放相关资源
    seff_coroutine_delete(co);
    return 0;
}

