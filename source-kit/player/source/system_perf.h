#ifndef SYSTEM_PERF_H
#define SYSTEM_PERF_H
#include <stddef.h>
#include <stdio.h>
enum {PERF_OTHER,PERF_AUDIO,PERF_FM,PERF_PCM,PERF_SCENE,PERF_DRAW,PERF_READ,
      PERF_WORKER,PERF_VBLANK,PERF_WAIT,PERF_COUNT};
#ifdef __NDS__
void system_perf_start(void);
unsigned system_perf_enter(unsigned category);
void system_perf_leave(unsigned previous);
void system_perf_snapshot(unsigned out[PERF_COUNT]);
void system_perf_sample(const unsigned audio[17]);
int system_perf_write(void);
void system_perf_summary(char *,size_t);
extern unsigned system_perf_write_errno;
void system_perf_wait_vblank(void);
size_t system_perf_fread(void *,size_t,size_t,FILE *);
extern volatile unsigned system_perf_totals[PERF_COUNT];
#else
/* Host audio oracles use exactly the same arithmetic without hardware timing. */
static inline unsigned system_perf_enter(unsigned category){(void)category;return 0;}
static inline void system_perf_leave(unsigned previous){(void)previous;}
#define system_perf_fread fread
#endif
#endif
