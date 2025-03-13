#ifndef SEFF_SEFF_TYPES_H
#define SEFF_SEFF_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define STACK_POLICY_SEGMENTED 

#define STACK_POLICY_SWITCH(segmented, fixed, vmmem) segmented

#define MAX_EFFECTS 64

typedef uint64_t effect_set;

typedef uint64_t effect_id;

#define RETURN_EFFECT_ID 0xFFFFFFFFFFFFFFFF

typedef struct _seff_request_t {
    effect_id effect;
    void *payload;
} seff_request_t;

typedef struct _seff_cont_t {
    void *stack_top;
    void *current_coroutine;
    void *rsp;
    void *rbp;
    void *ip;
    void *rbx;
    void *r12;
    void *r13;
    void *r14;
    void *r15;
} seff_cont_t;

typedef uint64_t seff_coroutine_state_t;
#define FINISHED ((seff_coroutine_state_t)0)
#define PAUSED ((seff_coroutine_state_t)1)
#define RUNNING ((seff_coroutine_state_t)2)

typedef struct _seff_stack_segment_t {
    struct _seff_stack_segment_t *prev;
    struct _seff_stack_segment_t *next;
    size_t size;
    void *canary;
    uint8_t padding[128];
} seff_stack_segment_t;

typedef seff_stack_segment_t *seff_frame_ptr_t;

typedef struct _seff_coroutine_t {
    seff_frame_ptr_t frame_ptr;
    seff_cont_t resume_point;
    seff_coroutine_state_t state;
    struct _seff_coroutine_t *parent_coroutine;
    effect_set handled_effects;
} seff_coroutine_t;


#endif
