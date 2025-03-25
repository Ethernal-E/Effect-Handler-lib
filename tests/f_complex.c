

#include "seff.h"
#include "mem/seff_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>
#include <pthread.h>


DEFINE_EFFECT(complex_yield, 0, void, { });


static pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;

static volatile int global = 0;


static inline void extra_work(void) {
    int sum = 0;
    for (int i = 0; i < 100; i++) {
        sum += i;
    }
    global += sum;
}


static void* complex_yield_coroutine(void* arg) {
    int64_t iterations = *(int64_t*)arg;
    for (int64_t i = 0; i < iterations; i++) {
        
        volatile int cal = i * 2;
        PERFORM(complex_yield, 0);
    }
    return (void*)(intptr_t)iterations;
}


static int64_t handle_complex_yield_loop(seff_coroutine_t* k) {
    seff_request_t req = seff_resume(k, NULL, HANDLES(complex_yield));
    while (req.effect != EFF_ID(return)) {
        switch (req.effect) {
            CASE_EFFECT(req, complex_yield, {
                
                pthread_mutex_lock(&sync_mutex);
                extra_work();
                pthread_mutex_unlock(&sync_mutex);
                req = seff_resume(k, NULL, HANDLES(complex_yield));
            })
        }
    }
    return (int64_t)(intptr_t)req.payload;
}

int main(int argc, char** argv) {
    
    int64_t iterations = (argc < 2) ? 10000 : atoll(argv[1]);

    
    char buffer[8192];
    setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));
    seff_coroutine_t* k = seff_coroutine_new(complex_yield_coroutine, &iterations);

    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    
    int64_t res = handle_complex_yield_loop(k);
    seff_coroutine_delete(k);

    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("iterations: %" PRId64 "\n", res);
    printf("Elapsed time: %f seconds\n", time);
    return 0;
}

