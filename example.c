#include "seff.h"
#include "tl_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

DEFINE_EFFECT(task, 1, void, { void *(*fn)(void *); void *arg; });
DEFINE_EFFECT(yield_task, 0, void, {});


volatile int worker_2_done = 0;

void *worker(void *param) {
    int task_id = (int)(intptr_t)param;
    if (task_id == 1) {
        while (!worker_2_done) { 
            printf("Worker 1: Waiting for Worker 2\n");
            PERFORM(yield_task);  
        }
        printf("Worker 1: Worker 2 is done\n");
    }

    for (int i = 0; i < 3; i++) {  
        printf("Worker %d: times %d\n", task_id, i);
        PERFORM(yield_task);  
    }

    

    
    if (task_id == 2) {
        printf("Worker 2: Done.\n");
        worker_2_done = 1;  
    }

    return NULL;
}


void *root(void *param) {
    for (int i = 1; i <= 4; i++) {  
        PERFORM(task, worker, (void *)(intptr_t)i);
    }
    return NULL;
}


void with_scheduler(seff_coroutine_t *initial_coroutine) {
    effect_set handles_scheduler = HANDLES(task) | HANDLES(yield_task);

    tl_queue_t queue;
    tl_queue_init(&queue, 10);
    tl_queue_push(&queue, (queue_elt_t)initial_coroutine);

    srand((unsigned int)time(NULL)); 

    while (!tl_queue_empty(&queue)) {
        seff_coroutine_t *tasks[10];
        int num_tasks = 0;

        
        while (!tl_queue_empty(&queue) && num_tasks < 10) {
            tasks[num_tasks++] = (seff_coroutine_t *)tl_queue_steal(&queue);
        }

       
        seff_coroutine_t *next = tasks[rand() % num_tasks];

        
        for (int i = 0; i < num_tasks; i++) {
            if (tasks[i] != next) {
                tl_queue_push(&queue, (queue_elt_t)tasks[i]);
            }
        }

        seff_request_t req = seff_handle(next, NULL, handles_scheduler);

        switch (req.effect) {
            CASE_EFFECT(req, task, {
                
                seff_coroutine_t *new_task = seff_coroutine_new(payload.fn, payload.arg);
                tl_queue_push(&queue, (queue_elt_t)new_task);
                tl_queue_push(&queue, (queue_elt_t)next);
                break;
            });
            CASE_EFFECT(req, yield_task, {
                
                tl_queue_push(&queue, (queue_elt_t)next);
                break;
            });
            CASE_RETURN(req, {
                printf("Complete coroutine: %p\n", (void*)next);
                seff_coroutine_delete(next);
                break;
            });
            default:
                printf("Error")
                seff_coroutine_delete(next);
                break;
        }
    }
}

int main(void) {
    seff_coroutine_t *root_task = seff_coroutine_new(root, NULL);
    if (root_task == NULL) {
        printf("Error");
        return 1;
    }
    with_scheduler(root_task);
    return 0;
}

