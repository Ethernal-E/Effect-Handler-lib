#define _GNU_SOURCE


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include "seff_mem.h"


// 期望的协程可用栈大小（例如64KB）
#define DEFAULT_DEFAULT_FRAME_SIZE (150 * 1024)

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif




#define GUARD_SIZE (PAGE_SIZE)                       // 一页作为 guard page（通常为 4KB）

#define STACK_EXPANSION_THRESHOLD (64)

#include "seff_mem_common.h"

// 向上取整到 PAGE_SIZE 的辅助函数
static inline size_t round_up(size_t size) {
    return (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

/*
 * 内存映射布局（假设栈向下增长）：
 *
 *    +------------------------------+ <-- region + total_size (映射高地址)
 *    |     Allowed 区 (可读写)       |
 *    |   [region+GUARD_SIZE, ... )    |
 *    +------------------------------+
 *    |     Guard page (不可访问)      |
 *    |   [region, region+GUARD_SIZE)  |
 *    +------------------------------+ <-- region (映射起始地址)
 *
 * 初始栈指针设为 region + total_size（经 16 字节对齐）。
 *
 * 采用 MAP_NORESERVE 使得 mmap 时不立即保留物理内存，利用 overcommit 策略。
 */
static void *g_stack_region = NULL;
static size_t g_allowed_size = 0; // Allowed 区大小（页对齐后）
static size_t g_total_size = 0;   // 总映射大小 = GUARD_SIZE + allowed_size

/*
 * 初始化虚拟内存栈。
 * frame_size 为期望的允许区域大小（未必页对齐），内部会向上取整。
 * 返回映射起始地址，同时通过 rsp 返回初始栈指针（位于映射顶部）。
 */
void *init_stack_frame(size_t frame_size, char **rsp) {
    g_allowed_size = round_up(frame_size);
    g_total_size = GUARD_SIZE + g_allowed_size;
    // 使用 MAP_NORESERVE 标志，启用 overcommit 策略
    void *region = mmap(NULL, g_total_size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    // 设置底部的 Guard page（映射起始处）为不可访问
    if (mprotect(region, GUARD_SIZE, PROT_NONE) != 0) {
        perror("mprotect guard page failed");
        exit(1);
    }
    g_stack_region = region;
    char *initial_sp = (char*)region + g_total_size;
    // 16 字节对齐
    initial_sp = (char*)((uintptr_t)initial_sp & ~((uintptr_t)0xF));
    if (rsp)
        *rsp = initial_sp;
    return region;
}

/*
 * 释放虚拟内存栈。
 */
void release_stack_frame(void *stack) {
    if (munmap(stack, g_total_size) != 0) {
        perror("munmap failed");
    }
}

/*
 * 扩展虚拟内存栈。
 * new_frame_size 为新的允许区域大小（未必页对齐），内部会向上取整。
 * 扩展后，全局变量会更新，并通过 new_rsp 返回新的初始栈指针。
 */
void expand_stack_frame(size_t new_frame_size, char **new_rsp) {
    size_t new_allowed_size = round_up(new_frame_size);
    size_t new_total_size = GUARD_SIZE + new_allowed_size;
    void *new_region = mremap(g_stack_region, g_total_size, new_total_size, MREMAP_MAYMOVE);
    if (new_region == MAP_FAILED) {
         perror("mremap failed");
         exit(1);
    }
    g_stack_region = new_region;
    g_allowed_size = new_allowed_size;
    g_total_size = new_total_size;
    // Guard page位于映射起始处，mremap后一般属性保持不变
    char *new_sp = (char*)new_region + g_total_size;
    new_sp = (char*)((uintptr_t)new_sp & ~((uintptr_t)0xF));
    if (new_rsp)
         *new_rsp = new_sp;
}

/*
 * 主动检测剩余栈空间，如果当前栈指针距离允许区域下界太近则扩展栈。
 * 由于栈向下增长，允许区域为 [g_stack_region+GUARD_SIZE, g_stack_region+g_total_size)。
 * 当当前栈指针低于 (g_stack_region+GUARD_SIZE + STACK_EXPANSION_THRESHOLD) 时，
 * 认为剩余空间不足，从而主动扩展。
 */
void ensure_stack_space(void) {
    char dummy;
    char *current_sp = &dummy;
    char *allowed_bottom = (char*)g_stack_region + GUARD_SIZE;
    if (current_sp < allowed_bottom + STACK_EXPANSION_THRESHOLD) {
        size_t new_allowed = g_allowed_size * 2; // 例如扩展为2倍
        char *new_sp = NULL;
        fprintf(stderr, "Proactively expanding stack: new allowed size = %zu bytes\n", new_allowed);
        expand_stack_frame(new_allowed, &new_sp);
        fprintf(stderr, "New stack pointer: %p\n", (void*)new_sp);
    }
}


