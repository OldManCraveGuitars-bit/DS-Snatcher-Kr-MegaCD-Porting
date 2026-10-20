#ifndef RESOURCE_TASKS_H
#define RESOURCE_TASKS_H
#include "resource_scene.h"
/* Original SUBCODE FFC4 work queue at9940. VBlank callers control the
 * invocation cadence; no recorded frame times or guessed audio durations. */
typedef struct {
    uint8_t commands[512];
    uint16_t index,counter,phase,timeout; /* 9B40,9B42,7856,784E */
    uint8_t active,caption_reset,error; /* 9B44,9717,port error */
} resource_tasks_state;
enum { RESOURCE_TASK_PCM_START=1,RESOURCE_TASK_CD_PAUSE=2,RESOURCE_TASK_FILE_START=3,
       RESOURCE_TASK_CD_START=4,RESOURCE_TASK_ACTOR_RESTORE=5 };
typedef struct {
    void *context;
    bool (*request)(void *context,unsigned kind,uint32_t destination,unsigned argument);
    int16_t cd_status;
    bool file_busy;
} resource_tasks_io;
bool resource_tasks_begin(resource_tasks_state *state,const void *commands,size_t bytes);
bool resource_tasks_tick(resource_tasks_state *state,resource_scene_state *scene,
    resource_scene_devices *devices,const resource_tasks_io *io);
#endif
