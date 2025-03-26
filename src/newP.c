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
#define DEFAULT_DEFAULT_FRAME_SIZE (200 * 1024)

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif




#define GUARD_SIZE (PAGE_SIZE)                       // 一页作为 guard page（通常为 4KB）



#include "seff_mem_common.h"

// 全局变量：保存映射的起始地址、Allowed 区大小以及总映射大小
static void *g_stack_region = NULL;
static size_t g_allowed_size = 0; // Allowed 区大小（向上取整后）
static size_t g_total_size = 0;   // 总映射大小 = GUARD_SIZE + g_allowed_size

// 保存旧的 SIGSEGV 信号处理器（用于链式调用或退出时恢复）
static struct sigaction old_sigsegv_action;

// 备用信号栈的全局变量
static stack_t g_alt_stack;

// 向上取整到 PAGE_SIZE 的辅助函数
static inline size_t round_up(size_t size) {
    return (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

/*
 * SIGSEGV 信号处理器：当访问未提交的页面时会触发。
 * 如果 fault address 在 Allowed 区内，则将该页通过 mprotect 设置为可读写，
 * 从而实现按需提交内存。
 */
static void segv_handler(int sig, siginfo_t *si, void *unused) {
    fprintf(stderr, "New stack pointer: 1");
    (void)sig; (void)unused;
    void *addr = si->si_addr;
    uintptr_t region_start = (uintptr_t)g_stack_region;
    uintptr_t allowed_start = region_start + GUARD_SIZE;
    uintptr_t allowed_end = region_start + g_total_size;
    uintptr_t fault_addr = (uintptr_t)addr;

    // 如果 fault_addr 位于 Allowed 区内，则按页对齐后提交该页
    if (fault_addr >= allowed_start && fault_addr < allowed_end) {
        void *page_start = (void*)(fault_addr & ~(PAGE_SIZE - 1));
        fprintf(stderr, "New stack pointer: 2");
        if (mprotect(page_start, PAGE_SIZE, PROT_READ | PROT_WRITE) == 0) {
            fprintf(stderr, "New stack pointer: 3");
            // 提交成功，直接返回后程序将重新执行触发 fault 的指令
            return;
        } else {
            perror("mprotect in segv_handler failed");
            exit(1);
        }
    }
    // 不在我们的管理区域，调用旧的处理器或默认处理
    if (old_sigsegv_action.sa_sigaction) {
        fprintf(stderr, "New stack pointer: 4");
        old_sigsegv_action.sa_sigaction(sig, si, unused);
    } else {
        fprintf(stderr, "New stack pointer: 5");
        signal(sig, SIG_DFL);
        raise(sig);
    }
}

/*
 * 初始化备用信号栈。
 * 分配一块足够的内存，并调用 sigaltstack 安装备用栈。
 */
static void init_alt_stack(void) {
    // 备用栈大小，建议至少使用 MINSIGSTKSZ 的两倍
    size_t alt_stack_size = MINSIGSTKSZ * 2;
    void *alt_sp = malloc(alt_stack_size);
    if (!alt_sp) {
        perror("malloc for alt stack failed");
        exit(1);
    }
    g_alt_stack.ss_sp = alt_sp;
    g_alt_stack.ss_size = alt_stack_size;
    g_alt_stack.ss_flags = 0;
    if (sigaltstack(&g_alt_stack, NULL) != 0) {
        perror("sigaltstack failed");
        exit(1);
    }
}

/*
 * 释放备用信号栈：禁用备用栈并释放内存。
 */
static void release_alt_stack(void) {
    stack_t disable_stack;
    disable_stack.ss_flags = SS_DISABLE;
    disable_stack.ss_sp = NULL;
    disable_stack.ss_size = 0;
    if (sigaltstack(&disable_stack, NULL) != 0) {
        perror("disabling alt stack failed");
    }
    free(g_alt_stack.ss_sp);
    g_alt_stack.ss_sp = NULL;
    g_alt_stack.ss_size = 0;
}

/*
 * 初始化虚拟内存栈（按需提交版）。
 * frame_size 为预期的 Allowed 区大小（未必页对齐），内部会向上取整。
 * 返回映射起始地址，同时通过 rsp 返回初始栈指针（位于映射顶部）。
 *
 * 布局：
 *    +------------------------------+ <-- region + g_total_size (映射高地址)
 *    |      Allowed 区（按需提交）     |
 *    |  [region+GUARD_SIZE, ... )      |
 *    +------------------------------+
 *    |      Guard page (不可访问)      |
 *    | [region, region+GUARD_SIZE)    |
 *    +------------------------------+ <-- region (映射起始地址)
 *
 * 注意：这里不预先提交任何页面，整个区域（包括 Allowed 区）初始均为 PROT_NONE，
 * 所以第一次访问时就会触发 SIGSEGV，从而进入 segv_handler 提交页面。
 */
void *init_stack_frame(size_t frame_size, char **rsp) {
    g_allowed_size = round_up(frame_size);
    g_total_size = GUARD_SIZE + g_allowed_size;
    // 预留整个区域，初始时全部设为不可访问（PROT_NONE）
    void *region = mmap(NULL, g_total_size,
                        PROT_NONE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    g_stack_region = region;

    // 安装备用信号栈
    init_alt_stack();

    // 安装 SIGSEGV 信号处理器，实现按需提交
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;  // 注意：使用 SA_ONSTACK 使 handler 在备用栈上运行
    sa.sa_sigaction = segv_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, &old_sigsegv_action) != 0) {
        perror("sigaction failed");
        exit(1);
    }

    // 设定初始栈指针：映射区域的高地址（栈向下增长），并 16 字节对齐
    char *initial_sp = (char*)region + g_total_size;
    initial_sp = (char*)((uintptr_t)initial_sp & ~((uintptr_t)0xF));
    if (rsp)
        *rsp = initial_sp;
    return region;
}

/*
 * 释放虚拟内存栈，并恢复旧的 SIGSEGV 信号处理器，同时释放备用信号栈。
 */
void release_stack_frame(void *stack) {
    (void)stack; // 此处使用全局变量 g_stack_region 和 g_total_size
    sigaction(SIGSEGV, &old_sigsegv_action, NULL);
    release_alt_stack();
    if (munmap(g_stack_region, g_total_size) != 0) {
        perror("munmap failed");
    }
}
