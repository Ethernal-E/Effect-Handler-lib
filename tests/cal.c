#include "seff.h"
#include "seff_types.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DEFINE_EFFECT(input, 0, void, {char *op; double *inp1; double *inp2;});

DEFINE_EFFECT(output, 1, void, {double out; });

DEFINE_EFFECT(error, 2, void, {const char *mes;});







void *cal(void *arg) {
    char op;
    double num1, num2, result;
    
    while (1) {
        PERFORM(input, &op, &num1, &num2);
        
        
        switch (op) {
            case '+':
                result = num1 + num2;
                break;
            case '-':
                result = num1 - num2;
                break;
            case '*':
                result = num1 * num2;
                break;
            case '/':
                if (num2 != 0) {
                    result = num1 / num2;
                } else {
                    PERFORM(error, "can't divide by 0 !");
                    break;
                }
                break;
            default:
                PERFORM(error, "Invalid operator");
                break;
        }
        
        
        PERFORM(output, result);
    }
    
    return NULL;
}









void *handle_cal_effects(seff_coroutine_t *coroutine) {
    seff_request_t req = seff_resume(coroutine, NULL, HANDLES(input) | HANDLES(output) | HANDLES(error));
    
    switch (req.effect) {
        CASE_EFFECT(req, input, {
            
            printf("Enter op and num1 num2      ");
            scanf(" %c %lf %lf", payload.op, payload.inp1, payload.inp2);
            return handle_cal_effects(coroutine);
        })
        CASE_EFFECT(req, output, {
            printf("Result: %lf\n", payload.out);
            return handle_cal_effects(coroutine);
        })
        CASE_EFFECT(req, error, {
            printf("Error: %s\n", payload.mes);
            return handle_cal_effects(coroutine);
        })
        CASE_RETURN(req, {
            return NULL;
        })
    }
    
    return NULL;
}






int main(void) {
    seff_coroutine_t *cal_coroutine = seff_coroutine_new(cal, NULL);
    handle_cal_effects(cal_coroutine);
    seff_coroutine_delete(cal_coroutine);
    
    return 0;
}
