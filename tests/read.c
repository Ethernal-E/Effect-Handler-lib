/*#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "scheff.h"
#include "tl_queue.h"
#include "seff.h"

// 定义 future 和 await 效果
DEFINE_EFFECT(future, 2, void*, { void *(*fn)(void *); void *arg; });
DEFINE_EFFECT(await, 3, void*, { seff_coroutine_t *task; });
#define SEFF_EFFECT_RETURN 0


// 一个简单的异步函数，用来计算任务
void* async_task(void* param) {
    int64_t* num = (int64_t*)param;
    printf("Computing result for input: %" PRId64 "\n", *num);
    *num = (*num) * (*num); // 简单的平方计算
    return param;
}

// 启动一个新的任务
void* start_task(void* param) {
    int64_t* num = (int64_t*)param;
    printf("Starting a new task for %" PRId64 "\n", *num);
    seff_coroutine_t* task = (seff_coroutine_t*)PERFORM(future, async_task, param);
    int64_t* result = (int64_t*)PERFORM(await, task);
    printf("Task complete, result is: %" PRId64 "\n", *result);
    return NULL;
}

// 调度器函数，用于处理 future 和 await 效果
void with_scheduler(seff_coroutine_t *initial_coroutine) {
    effect_set handles_scheduler = HANDLES(future) | HANDLES(await);
    tl_queue_t queue;
    tl_queue_init(&queue, 5);
    tl_queue_push(&queue, (queue_elt_t)initial_coroutine);

    while (!tl_queue_empty(&queue)) {
        seff_coroutine_t *next = (seff_coroutine_t *)tl_queue_steal(&queue);
        seff_request_t req = seff_resume(next, NULL, handles_scheduler);

        switch (req.effect) {
            CASE_EFFECT(req, future, {
                // 创建新的协程并加入队列
                seff_coroutine_t *new = seff_coroutine_new(payload.fn, payload.arg);
                tl_queue_push(&queue, (struct task_t *)new);
                req.payload = new;
                break;
            })
            CASE_EFFECT(req, await, {
                // 等待协程完成并获取结果
                seff_request_t req_result = seff_resume(payload.task, NULL, handles_scheduler);
                if (req_result.effect == SEFF_EFFECT_RETURN) {
                    req.payload = req_result.payload;
                    seff_coroutine_delete(payload.task);
                } else {
                    tl_queue_push(&queue, (struct task_t *)payload.task);
                    tl_queue_push(&queue, (struct task_t *)next);
                }
                break;
            })
            CASE_RETURN(req, {
                // 任务完成，释放协程资源
                seff_coroutine_delete(next);
                break;
            })
        }
    }
}

// 主程序入口
int main(void) {
    int64_t value = 42;
    with_scheduler(seff_coroutine_new(start_task, &value));
    return 0;
}
*/


#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include "scheff.h"
#include "seff.h"

// Define future and await effects
DEFINE_EFFECT(future, 2, void*, { void *(*fn)(void *); void *arg; });
DEFINE_EFFECT(await, 3, void*, { seff_coroutine_t *task; });
#define SEFF_EFFECT_RETURN 0

// Asynchronous task that computes the square of a number
void* async_task(void* param) {
    int64_t* num = (int64_t*)param;
    printf("Computing result for input: %" PRId64 "\n", *num);
    *num = (*num) * (*num);  // Square calculation
    return param;  // Return the result
}

// Start a new task with coroutines
void* start_task(void* param) {
    int64_t* num = (int64_t*)param;
    printf("Starting a new task for %" PRId64 "\n", *num);

    // Perform the future effect to start async_task
    seff_coroutine_t* task = (seff_coroutine_t*)PERFORM(future, async_task, param);

    // Await the result of async_task
    int64_t* result = (int64_t*)PERFORM(await, task);

    // Print result if successfully retrieved
    if (result != NULL) {
        printf("Task complete, result is: %" PRId64 "\n", *result);
    } else {
        printf("Error: Task did not return a valid result.\n");
    }
    return NULL;
}

// Scheduler function to handle future and await effects
void with_scheduler(seff_coroutine_t *initial_coroutine) {
    effect_set handles_scheduler = HANDLES(future) | HANDLES(await);

    // Resume and process each coroutine
    seff_coroutine_t *next = initial_coroutine;
    while (next != NULL) {
        seff_request_t req = seff_resume(next, NULL, handles_scheduler);

        switch (req.effect) {
            CASE_EFFECT(req, future, {
                // Start async_task as a new coroutine
                seff_coroutine_t *new_task = seff_coroutine_new(payload.fn, payload.arg);
                req.payload = new_task;
                next = new_task;  // Move to async_task coroutine
                break;
            })
            CASE_EFFECT(req, await, {
                // Await task completion and ensure we detect SEFF_EFFECT_RETURN
                seff_request_t req_result = seff_resume(payload.task, NULL, handles_scheduler);
                
                if (req_result.effect == SEFF_EFFECT_RETURN) {
                    req.payload = req_result.payload;  // Pass result to start_task
                    seff_coroutine_delete(payload.task);  // Clean up
                    next = NULL;  // Exit after task completion
                } else {
                    next = payload.task;  // Reattempt awaiting task completion
                }
                break;
            })
            CASE_RETURN(req, {
                // Task is completed and result is printed
                seff_coroutine_delete(next);
                next = NULL;  // End scheduler loop
                break;
            })
        }
    }
}

// Main function to start the coroutine scheduler
int main(void) {
    int64_t value = 42;
    printf("Starting the scheduler...\n");
    seff_coroutine_t* main_task = seff_coroutine_new(start_task, &value);
    with_scheduler(main_task);  // Start the scheduler with the main task
    return 0;
}

