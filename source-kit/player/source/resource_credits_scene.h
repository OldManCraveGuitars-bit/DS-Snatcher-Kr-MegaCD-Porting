#ifndef RESOURCE_CREDITS_SCENE_H
#define RESOURCE_CREDITS_SCENE_H
#include "resource_credits.h"
#include "resource_opening.h"
#include "resource_opening_presentation.h"
typedef struct {
    resource_opening_state video;
    resource_opening_presentation presentation;
    resource_credits_state control;
} resource_credits_scene;
bool resource_credits_scene_begin(resource_credits_scene *,const void *,size_t,
    const void *,size_t,uint32_t);
bool resource_credits_scene_tick(resource_credits_scene *,uint32_t,uint16_t,uint8_t,uint8_t);
#endif
