#include "seff.h"
#include "mem/seff_mem.h"
#include "seff_types.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


DEFINE_EFFECT(async_op, 0, void, { int64_t x; });


static inline int64_t concurrentOperation(int64_t x, int64_t y) {
    return (x + y) ^ 0xABCDEF;
}


static int64_t handleAsyncOpRec(seff_coroutine_t *k) {
    seff_request_t req = seff_resume(k, NULL, HANDLES(async_op));
    switch (req.effect) {
        CASE_EFFECT(req, async_op, {
            return concurrentOperation(payload.x, handleAsyncOpRec(k));
        })
        CASE_RETURN(req, {
            return (int64_t) payload.result;
        })
    }
    return -1;
}


typedef struct thread_args_t {
    int64_t iterations; 
    int thread_id;      
} thread_args_t;


static void* async_loop(void* arg) {
    thread_args_t* args = (thread_args_t*) arg;
    for (int64_t i = args->iterations; i > 0; i--) {
        PERFORM(async_op, i);
    }
    
    return (void*)(intptr_t)args->thread_id;
}


static int64_t run_async(int64_t iterations, int thread_id) {
    thread_args_t args = {
        .iterations = iterations,
        .thread_id = thread_id
    };
    seff_coroutine_t *k = seff_coroutine_new(async_loop, &args);
    int64_t result = handleAsyncOpRec(k);
    seff_coroutine_delete(k);
    return result;
}


void* thread_func(void* arg) {
    thread_args_t* args = (thread_args_t*) arg;
    int64_t local_result = 0;
    
    for (int i = 0; i < 1000; i++) {
        local_result += run_async(args->iterations, args->thread_id);
    }
    return (void*)(intptr_t)local_result;
}

int main(int argc, char** argv) {
   
    int num_threads = (argc < 2) ? 4 : atoi(argv[1]);
    int64_t iterations = (argc < 3) ? 10000 : atoll(argv[2]);

    pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
    thread_args_t* targs = malloc(num_threads * sizeof(thread_args_t));

    
    for (int i = 0; i < num_threads; i++) {
        targs[i].iterations = iterations;
        targs[i].thread_id = i + 1;
        pthread_create(&threads[i], NULL, thread_func, &targs[i]);
    }

    int64_t final_result = 0;
    
    for (int i = 0; i < num_threads; i++) {
        void* res;
        pthread_join(threads[i], &res);
        final_result += (int64_t)(intptr_t)res;
    }

    free(threads);
    free(targs);

    
    char buffer[8192];
    setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));
    printf("Final aggregated result: %ld\n", final_result);
    return 0;
}

