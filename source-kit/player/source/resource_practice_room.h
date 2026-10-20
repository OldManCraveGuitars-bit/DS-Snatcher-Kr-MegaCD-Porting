#ifndef RESOURCE_PRACTICE_ROOM_H
#define RESOURCE_PRACTICE_ROOM_H
#include "resource_practice.h"
#include "resource_room.h"
/* The caller retains the original H06 room, including its native actors,
 * palette jobs and sound devices, across introduction/practice/results. */
bool resource_practice_room_begin(resource_practice *,resource_room *);
bool resource_native_combat_room_begin(resource_practice *,resource_room *,unsigned stage);
bool resource_practice_room_tick(resource_practice *,resource_room *,unsigned held,unsigned pressed);
bool resource_practice_room_done(const resource_practice *);
bool resource_practice_room_reset_begin(resource_practice *,resource_room *);
bool resource_practice_room_reset_tick(resource_practice *,resource_room *,bool *complete);
#endif
