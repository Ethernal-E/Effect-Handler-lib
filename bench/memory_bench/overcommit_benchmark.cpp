#include "memory_common.hpp"
#include "seff.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>

#define OVERCOMMIT_STACK_SIZE (8 * 1024 * 1024) // 8MB

void *allocate_overcommit_stack(size_t size) {
    void *stack = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    return stack;
}

void free_overcommit_stack(void *stack, size_t size) {
    munmap(stack, size);
}

template <size_t padding>
void *deep_coroutine(void *arg) {
    char arr[padding];
    int64_t depth = (int64_t)arg;

    if (depth == 0) {
        seff_coroutine_t *self = seff_current_coroutine();
        volatile bool loop = true;
        while (loop) {
            seff_yield(self, 0, nullptr);
        }
        return nullptr;
        
    } else {
        return deep_coroutine<padding>((void *)(depth - 1));
        
    }
}

void run_benchmark(int instances, int64_t depth, int padding) {
    seff_start_fun_t *fn;

    switch (padding) {
#define X(n)                    \
    case n:                     \
        fn = deep_coroutine<n>; \
        break;
        PADDING_SIZES
#undef X
    default:
        printf("Incorrect padding\n");
        return;
    }

    seff_coroutine_t **coroutines = new seff_coroutine_t *[instances];
    void **stacks = new void *[instances];

    for (int i = 0; i < instances; i++) {
        stacks[i] = allocate_overcommit_stack(OVERCOMMIT_STACK_SIZE);
        coroutines[i] = seff_coroutine_new_sized(fn, (void *)depth, OVERCOMMIT_STACK_SIZE);
        seff_resume_handling_all(coroutines[i], nullptr);
    }

    // 释放资源
    for (int i = 0; i < instances; i++) {
        seff_coroutine_delete(coroutines[i]);
        free_overcommit_stack(stacks[i], OVERCOMMIT_STACK_SIZE);
    }
    
    delete[] coroutines;
    delete[] stacks;
}

