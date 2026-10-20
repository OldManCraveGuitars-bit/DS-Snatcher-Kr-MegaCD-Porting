#ifndef RESOURCE_PRACTICE_H
#define RESOURCE_PRACTICE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Original SUBCODE stage0 controller. Actor animation remains in the shared
 * original resource engine; this module never supplies poses or a timeline.
 * ram maps7800..79FF; actors maps7A00..8DFF, both big endian. */
typedef struct {
    uint8_t ram[512];
    uint8_t *actors;
    const uint8_t *resource,*subcode;
    size_t resource_bytes,subcode_bytes;
    void *context;
    bool (*request)(void *,uint32_t entry,uint32_t argument,unsigned channel);
    uint16_t scratch_word_2a,host_ready,host_request,host_x,host_y;
    uint8_t pcm_flags;
    unsigned error;
} resource_practice;
bool resource_practice_bind(resource_practice *,uint8_t actors[5120],
    const void *resource,size_t,const void *subcode,size_t);
/* Source entrypoints are exposed for independent instruction-oracle tests.
 * Full controller dispatch is1208E; the current contract is original stage0. */
bool resource_practice_call(resource_practice *,uint32_t entry);
bool resource_practice_score_reset(resource_practice *,bool *complete);
#endif
