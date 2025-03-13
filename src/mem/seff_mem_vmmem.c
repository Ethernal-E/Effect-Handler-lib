/*#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdint.h>

#include "seff_mem.h"

#define DEFAULT_DEFAULT_FRAME_SIZE (64 * 1024)  // 64KB，期望的协程可用栈大小
#define REGION_SIZE (DEFAULT_DEFAULT_FRAME_SIZE + sizeof(void*) + 128)  // 实际分配的区域要比 frame 大一些

#define PAGE_SIZE 4096
#include "seff_mem_common.h"

// 分配一块 overcommit 栈区域，大小为 REGION_SIZE
void *allocate_overcommit_stack() {
    void *addr = mmap(NULL, REGION_SIZE, 
                      PROT_NONE, 
                      MAP_PRIVATE | MAP_ANONYMOUS, 
                      -1, 0);
    if (addr == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    return addr;
}

void page_fault_handler(int sig, siginfo_t *info, void *context) {
    void *fault_addr = info->si_addr;  
    void *aligned_addr = (void *)((uintptr_t)fault_addr & ~(PAGE_SIZE - 1));
    if (mprotect(aligned_addr, PAGE_SIZE, PROT_READ | PROT_WRITE) != 0) {
        perror("mprotect failed");
        exit(1);
    }
    memset(aligned_addr, 0, PAGE_SIZE);
}

void setup_signal_handler() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = page_fault_handler;
    sigaction(SIGSEGV, &sa, NULL);
}


seff_frame_ptr_t init_stack_frame(size_t frame_size, char **rsp) {
    size_t meta_size = sizeof(void*);
    if (REGION_SIZE < frame_size + meta_size) {
       fprintf(stderr, "Not enough space for frame and metadata\n");
       exit(1);
    }
    // 为当前协程分配独立的内存区域
    char *region = (char *)allocate_overcommit_stack();
    // 初始栈顶在区域末尾
    char *raw_top = region + REGION_SIZE;
    // 预留 meta_size 空间存放 region 指针，并预留 frame_size 空间供协程使用
    char *meta_addr = raw_top - (frame_size + meta_size);
    *((void **)meta_addr) = region;
    // 可用的栈指针为 meta_addr 后面的部分
    char *final_stack = meta_addr + meta_size;
    // 调整为 16 字节对齐
    while (((uintptr_t)final_stack) % 16 != 0) {
         final_stack++;
    }
    if (rsp) {
         *rsp = final_stack;
    }
    return (seff_frame_ptr_t)final_stack;
}

void release_stack_frame(seff_frame_ptr_t stack) {
    size_t meta_size = sizeof(void*);
    char *final_stack = (char*)stack;
    // 元数据存放在 final_stack 前 meta_size 字节处
    char *meta_addr = final_stack - meta_size;
    void *region = *((void **)meta_addr);
    if (munmap(region, REGION_SIZE) != 0) {
         perror("munmap failed");
    }
}
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdint.h>

#include "seff_mem.h"

// 期望的协程可用栈大小（例如64KB）
#define DEFAULT_DEFAULT_FRAME_SIZE (64 * 1024)
// 实际分配的区域大小，比期望的栈大小略大，以存储元数据等
#define REGION_SIZE (DEFAULT_DEFAULT_FRAME_SIZE + sizeof(void*) + 128)

#define PAGE_SIZE 4096
#include "seff_mem_common.h"

/* 
 * 分配一块 overcommit 栈区域，大小为 REGION_SIZE。
 * 注意：这里用 mmap 分配的内存需要在释放时使用 munmap。
 */
void *allocate_overcommit_stack() {
    void *addr = mmap(NULL, REGION_SIZE,
                      PROT_NONE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);
    if (addr == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    return addr;
}

/*
 * SIGSEGV 信号处理函数：当访问未映射的页面时，解除保护并初始化该页
 */
void page_fault_handler(int sig, siginfo_t *info, void *context) {
    void *fault_addr = info->si_addr;
    void *aligned_addr = (void *)((uintptr_t)fault_addr & ~(PAGE_SIZE - 1));
    if (mprotect(aligned_addr, PAGE_SIZE, PROT_READ | PROT_WRITE) != 0) {
        perror("mprotect failed");
        exit(1);
    }
    memset(aligned_addr, 0, PAGE_SIZE);
}

/*
 * 安装 SIGSEGV 信号处理器，确保虚拟内存栈按需映射页面
 */
void setup_signal_handler() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = page_fault_handler;
    sigaction(SIGSEGV, &sa, NULL);
}

/*
 * 为当前协程分配一块独立的 overcommit 栈区域。
 *
 * 逻辑说明：
 * 1. 分配 REGION_SIZE 大小的区域。
 * 2. 计算 raw_top = region + REGION_SIZE（区域末尾）。
 * 3. 保留 (frame_size + meta_size) 字节，其中 meta_size 用于存储原始区域指针，
 *    计算 meta_addr = raw_top - (frame_size + meta_size)。
 * 4. 将 region 指针存入 meta_addr，以便后续释放时调用 munmap。
 * 5. 可用栈指针从 meta_addr + meta_size 开始，然后向上取 16 字节对齐。
 * 6. 检查对齐后地址是否超过 raw_top，如果超过则报错。
 */
seff_frame_ptr_t init_stack_frame(size_t frame_size, char **rsp) {
    size_t meta_size = sizeof(void*);
    if (REGION_SIZE < frame_size + meta_size) {
       fprintf(stderr, "Not enough space for frame and metadata\n");
       exit(1);
    }
    // 分配区域
    char *region = (char *)allocate_overcommit_stack();
    // 区域末尾
    char *raw_top = region + REGION_SIZE;
    // 预留 (frame_size + meta_size) 空间：meta_addr 用于存储 region 指针
    char *meta_addr = raw_top - (frame_size + meta_size);
    *((void **)meta_addr) = region;
    // 可用栈从 meta_addr + meta_size 开始
    uintptr_t unaligned = (uintptr_t)(meta_addr + meta_size);
    // 向上对齐到16字节
    uintptr_t aligned = (unaligned + 15) & ~((uintptr_t)0xF);
    // 检查对齐后的地址是否超出区域
    if (aligned > (uintptr_t)raw_top) {
         fprintf(stderr, "Aligned stack pointer exceeds region\n");
         exit(1);
    }
    char *final_stack = (char *)aligned;
    if (rsp) {
         *rsp = final_stack;
    }
    return (seff_frame_ptr_t)final_stack;
}

/*
 * 释放协程的栈区域：
 * 根据传入的栈指针，回退 meta_size 得到保存 region 指针的位置，
 * 读取后调用 munmap 释放整个 REGION_SIZE 大小的区域。
 */
void release_stack_frame(seff_frame_ptr_t stack) {
    size_t meta_size = sizeof(void*);
    char *final_stack = (char*)stack;
    // 元数据存放在 final_stack 前 meta_size 字节处
    char *meta_addr = final_stack - meta_size;
    void *region = *((void **)meta_addr);
    if (munmap(region, REGION_SIZE) != 0) {
         perror("munmap failed");
    }
}
















