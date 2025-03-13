#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdint.h>

#include "seff_mem.h"

#define OVERCOMMIT_STACK_SIZE (100 * 1024 * 1024)

#define PAGE_SIZE 4096


void *allocate_overcommit_stack() {
    void *addr = mmap(NULL, OVERCOMMIT_STACK_SIZE, 
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


char *init_stack_frame(size_t frame_size) {
    static __thread char *stack_top = NULL;

    if (stack_top == NULL) {
        
        stack_top = (char *)allocate_overcommit_stack() + OVERCOMMIT_STACK_SIZE;
    }

    
    stack_top -= frame_size;

    
    while (((uintptr_t)stack_top) % 16 != 0) {
        stack_top -= 1;
    }

    return stack_top;
}


void release_stack_frame(size_t frame_size) {
    static __thread char *stack_top = NULL;

    if (stack_top == NULL) return; 

    
    stack_top += frame_size;

    
    madvise(stack_top, PAGE_SIZE, MADV_DONTNEED);
}
