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



#define DEFAULT_DEFAULT_FRAME_SIZE (150 * 1024)

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif




#define GUARD_SIZE (PAGE_SIZE)                       

#define STACK_EXPANSION_THRESHOLD (64)

#include "seff_mem_common.h"


static inline size_t round_up(size_t size) {
    return (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}


static void *g_stack_region = NULL;
static size_t g_allowed_size = 0; 
static size_t g_total_size = 0;   


void *init_stack_frame(size_t frame_size, char **rsp) {
    g_allowed_size = round_up(frame_size);
    g_total_size = GUARD_SIZE + g_allowed_size;
    
    void *region = mmap(NULL, g_total_size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    
    if (mprotect(region, GUARD_SIZE, PROT_NONE) != 0) {
        perror("mprotect guard page failed");
        exit(1);
    }
    g_stack_region = region;
    char *initial_sp = (char*)region + g_total_size;
    
    initial_sp = (char*)((uintptr_t)initial_sp & ~((uintptr_t)0xF));
    if (rsp)
        *rsp = initial_sp;
    return region;
}


void release_stack_frame(void *stack) {
    if (munmap(stack, g_total_size) != 0) {
        perror("munmap failed");
    }
}


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
    
    char *new_sp = (char*)new_region + g_total_size;
    new_sp = (char*)((uintptr_t)new_sp & ~((uintptr_t)0xF));
    if (new_rsp)
         *new_rsp = new_sp;
}


void ensure_stack_space(void) {
    char dummy;
    char *current_sp = &dummy;
    char *allowed_bottom = (char*)g_stack_region + GUARD_SIZE;
    if (current_sp < allowed_bottom + STACK_EXPANSION_THRESHOLD) {
        size_t new_allowed = g_allowed_size * 2; 
        char *new_sp = NULL;
        fprintf(stderr, "Proactively expanding stack: new allowed size = %zu bytes\n", new_allowed);
        expand_stack_frame(new_allowed, &new_sp);
        fprintf(stderr, "New stack pointer: %p\n", (void*)new_sp);
    }
}


