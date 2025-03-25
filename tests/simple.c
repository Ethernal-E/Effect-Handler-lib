#include "seff.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void *fn(void *arg) {
    seff_yield(seff_current_coroutine(), 0, NULL);
    return NULL;
}

int main(void) {
    seff_coroutine_t *k = seff_coroutine_new(fn, NULL);
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    
    
    
    seff_resume_handling_all(k, NULL);
    seff_resume_handling_all(k, NULL);
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("time: %f seconds\n", time);
}
