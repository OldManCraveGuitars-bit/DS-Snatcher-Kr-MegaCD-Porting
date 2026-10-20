#ifndef RESOURCE_PROGRAM_H
#define RESOURCE_PROGRAM_H
#include "resource_tasks.h"
/* Bounded original SP06 prefix interpreter. No recording/timeline input.
 * Expressions append original FFC4 tasks; 2B branches are followed only
 * after the preceding queue finishes. Dialogue is an explicit handoff. */
typedef struct {
    const uint8_t *script;
    size_t bytes;
    uint16_t pc,scene_key,word79f0;
    uint8_t dialogue,error;
    /* Borrowed live state; validated expressions commit before queued tasks.
     * The owner must retain these buffers through every program call. */
    uint8_t *fields, *pcm_attenuation;
} resource_program_state;
bool resource_program_begin(resource_program_state *state,const void *script,
    size_t bytes,uint16_t pc,uint16_t scene_key,uint16_t word79f0);
/* 1=queue prepared, 2=dialogue handoff at pc, -1=invalid/unsupported.
 * Calling while a queue is active fails without replacing that queue. */
int resource_program_next(resource_program_state *state,resource_tasks_state *tasks);
#endif
