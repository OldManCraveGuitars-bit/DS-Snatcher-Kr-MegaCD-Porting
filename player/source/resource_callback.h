#ifndef RESOURCE_CALLBACK_H
#define RESOURCE_CALLBACK_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    const uint8_t *bank;
    size_t bytes;
    uint32_t directory,resource_offset,resource_size,base;
    uint16_t events,track,file_id;
    int16_t index;
    uint32_t executed;
} resource_callback_state;
typedef bool (*resource_callback_job)(void *context,unsigned kind,unsigned size,uint32_t address,uint32_t value);
bool resource_callback_bind(resource_callback_state *state,const void *data,size_t bytes);
/* Original dispatch executes at most one due callback per native VBlank.
 * Returns1 if executed,0 while waiting,-1 for unsupported/damaged input. */
int resource_callback_tick(resource_callback_state *state,uint32_t original_bcd_clock,resource_callback_job job,void *context);
/* DATA_D0 polls original CD status100 before advancing/resetting its clock.
 * The caller supplies the real stream/device status; zero means still waiting. */
int resource_callback_tick_cd(resource_callback_state *state,uint32_t original_bcd_clock,uint16_t cd_status,resource_callback_job job,void *context);
/* Original D0 negative-index initializer: phase is native7898 (0/1).
 * Does not increment the callback index before executing this routine. */
int resource_callback_initialize(resource_callback_state *state,uint16_t phase,uint16_t cd_status,resource_callback_job job,void *context);
#endif
