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



#include "seff_mem_common.h"


static void *g_stack_region = NULL;
static size_t g_allowed_size = 0; 
static size_t g_total_size = 0;   


static struct sigaction old_sigsegv_action;


static stack_t g_alt_stack;


static size_t committed_size = 0;


static inline size_t round_up(size_t size) {
    return (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}


static void segv_handler(int sig, siginfo_t *si, void *unused) {
    
    (void)sig; (void)unused;
    void *addr = si->si_addr;
    uintptr_t region_start = (uintptr_t)g_stack_region;
    uintptr_t allowed_start = region_start + GUARD_SIZE;
    uintptr_t allowed_end = region_start + g_total_size;
    uintptr_t fault_addr = (uintptr_t)addr;

    if (fault_addr >= allowed_start && fault_addr < allowed_end) {
        
        uintptr_t current_commit_end = allowed_start + committed_size;
        if (fault_addr < current_commit_end) {
           
            fprintf(stderr, "Fault in already committed region\n");
            exit(1);
        }
        
        size_t commit_size;
	if (committed_size == 0) {
    		
    		commit_size = PAGE_SIZE;
	} else {
    	
    		commit_size = committed_size;
	}
	
	if (committed_size + commit_size > g_allowed_size) {
    		commit_size = g_allowed_size - committed_size;
	}

	uintptr_t commit_start = allowed_start + committed_size;
	

	if (mprotect((void*)commit_start, commit_size, PROT_READ | PROT_WRITE) == 0) {
    		committed_size += commit_size;
    		
    		return;
	} else {
    		perror("mprotect in segv_handler failed");
    		exit(1);
	}
	
     }

        
        

    
    if (old_sigsegv_action.sa_sigaction) {
        fprintf(stderr, "Delegating fault to old handler\n");
        old_sigsegv_action.sa_sigaction(sig, si, unused);
    } else {
        fprintf(stderr, "No old handler, resetting signal and raising\n");
        signal(sig, SIG_DFL);
        raise(sig);
    }
}


static void init_alt_stack(void) {
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


void *init_stack_frame(size_t frame_size, char **rsp) {
    
    g_allowed_size = round_up(frame_size);
    g_total_size = GUARD_SIZE + g_allowed_size;
    
    void *region = mmap(NULL, g_total_size,
                        PROT_NONE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    g_stack_region = region;
    
    init_alt_stack();
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sa.sa_sigaction = segv_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, &old_sigsegv_action) != 0) {
        perror("sigaction failed");
        exit(1);
    }
    
    committed_size = 0;
    
    char *initial_sp = (char*)region + g_total_size;
    initial_sp = (char*)((uintptr_t)initial_sp & ~((uintptr_t)0xF));
    if (rsp)
        *rsp = initial_sp;
    return region;
}


void release_stack_frame(void *stack) {
    (void)stack; 
    sigaction(SIGSEGV, &old_sigsegv_action, NULL);
    release_alt_stack();
    if (munmap(g_stack_region, g_total_size) != 0) {
        perror("munmap failed");
    }
}
