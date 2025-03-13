#include "seff.h"
#include "mem/seff_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// 定义一个名为 stress_op 的效果，携带一个 int64_t 类型的载荷 x
DEFINE_EFFECT(stress_op, 0, void, { int64_t x; });

// 定义压力测试的二元操作，作为将当前效果值与后续结果组合的函数
// 这里我们采用 (x * 31 + y * 17 + 7) % 1000003 作为示例运算
static inline int64_t stressOperation(int64_t x, int64_t y) {
    return (x * 31 + y * 17 + 7) % 1000003;
}

// 递归处理协程中产生的 stress_op 效果，每遇到一次效果，就将载荷 x 与后续处理结果通过 stressOperation 组合
static int64_t handleStressOpRec(seff_coroutine_t *k) {
    seff_request_t req = seff_resume(k, NULL, HANDLES(stress_op));
    switch (req.effect) {
        CASE_EFFECT(req, stress_op, {
            return stressOperation(payload.x, handleStressOpRec(k));
        })
        CASE_RETURN(req, {
            return (int64_t) payload.result;
        })
    }
    return -1;
}

// 用于向协程传递参数的结构体
typedef struct stress_args_t {
    int64_t iterations;  // 每次协程中调用 stress_op 的次数
    int64_t initial;     // 初始状态值，用于累积计算
} stress_args_t;

// 协程主体函数：循环触发 stress_op 效果
static void* stress_loop(void* arg) {
    stress_args_t* args = (stress_args_t*) arg;
    for (int64_t i = args->iterations; i > 0; i--) {
        PERFORM(stress_op, i);
    }
    return (void*) args->initial;
}

// run_stress 函数负责创建协程、运行 stress_loop 并处理所有效果，然后释放协程
static int64_t run_stress(int64_t iterations, int64_t initial) {
    stress_args_t args = {
        .iterations = iterations,
        .initial = initial,
    };
    seff_coroutine_t* k = seff_coroutine_new(stress_loop, &args);
    int64_t result = handleStressOpRec(k);
    seff_coroutine_delete(k);
    return result;
}

// stress_test 函数重复调用 run_stress 多次，以模拟长时间压力测试
static int64_t stress_test(int64_t iterations, int64_t repeats) {
    int64_t result = 0;
    for (int64_t i = 0; i < repeats; i++) {
        result = run_stress(iterations, result);
    }
    return result;
}

int main(int argc, char** argv) {
    // 使用命令行参数配置每次协程中调用效果的迭代次数和重复次数
    int64_t iterations = (argc < 2) ? 10000 : atoll(argv[1]);
    int64_t repeats = (argc < 3) ? 1000 : atoll(argv[2]);
    int64_t res = stress_test(iterations, repeats);
    
    // 调整输出缓冲区大小以提高性能
    char buffer[8192];
    setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));
    printf("%ld\n", res);
    return 0;
}

