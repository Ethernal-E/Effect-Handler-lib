#include "seff.h"
#include "seff_types.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DEFINE_EFFECT(read_file, 0, char*, { const char* filename; });

void deal_loop(seff_coroutine_t *temp) {
    bool done = false;
    while (!done){
        seff_request_t req = seff_resume(temp, NULL, HANDLES(read_file));
        switch (req.effect){
            CASE_EFFECT(req, read_file, {
                char* text = "haloooooooooooo";
                req = seff_resume(temp, text, HANDLES(read_file));
                break;
            });
            CASE_RETURN(req, {
                done = true;
                break;
            });
        }
    }

    seff_coroutine_delete(temp);
}

void* read_print(void* param){
    char* text = PERFORM(read_file, "example.txt");
    printf("%s\n", text);
    return NULL;
}

int main(void){
    seff_coroutine_t *k = seff_coroutine_new(read_print, NULL);
    deal_loop(k);
    return 0;
}
