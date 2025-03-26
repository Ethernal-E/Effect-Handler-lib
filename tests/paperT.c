#include "seff.h"
#include "mem/seff_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>
#include <pthread.h>




DEFINE_EFFECT(fill_stack, 0, void, { });

void fill_stack_rec(int depth, int max_depth) {
    
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    if (depth < max_depth) {
        fill_stack_rec(depth + 1, max_depth);
    } else {
        
        PERFORM(fill_stack, 0);
    }
}

void *fill_fn(void *arg) {
    int max_depth = *(int*)arg;
    fill_stack_rec(0, max_depth);
    return NULL;
}

int main(int argc, char *argv[]) {
    int max_depth = 100;
    if (argc > 1) {
         max_depth = atoi(argv[1]);
    }
    
    seff_coroutine_t *co = seff_coroutine_new(fill_fn, &max_depth);
    
    seff_resume_handling_all(co, NULL);
    
    
    
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Stack expansion triggered: elapsed time = %f seconds\n", elapsed);
    
    seff_coroutine_delete(co);
    return 0;
}

