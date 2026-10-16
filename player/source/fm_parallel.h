#ifndef FM_PARALLEL_H
#define FM_PARALLEL_H
#include "ym2612_context.h"
typedef struct {
    chan_rend_context context;
    FM_CH channel;
    s32 output[128*2];
    volatile unsigned active;
    unsigned length;
} channel_job;
typedef struct {
    volatile unsigned ready;
    volatile unsigned worker_phase,worker_started,worker_completed;
    UINT16 lookup[256+5504];
    channel_job jobs[2];
} fm_shared;
#endif
