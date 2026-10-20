#ifndef RESOURCE_CREDITS_H
#define RESOURCE_CREDITS_H
#include "resource_callback.h"

/* DATA_Y16/290D4. Inputs are the original CD status,9C00 byte and7981
 * button edge. This controller does not own the CD clock or scene renderer. */
typedef struct {
    const uint8_t *data;
    size_t bytes;
    uint32_t clock;
    int16_t index;
    uint16_t phase,counter,finished;
} resource_credits_state;
bool resource_credits_bind(resource_credits_state *,const void *,size_t);
int resource_credits_tick(resource_credits_state *,uint32_t,uint16_t,
    uint8_t,uint8_t,resource_callback_job,void *);
#endif
