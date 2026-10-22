#pragma once
#include "resource_opening.h"
#include "resource_opening_presentation.h"
#include "resource_tasks.h"
#include "resource_contact.h"
#include "resource_flashlight.h"
/* Persistent original room actor/effect pipeline. Input data remains owned
 * by the caller. No bitmap frames or authored animation schedule. */
typedef struct {
    resource_opening_state video;
    resource_opening_presentation presentation;
    resource_tasks_state tasks;
    resource_scene_devices devices;
    unsigned scene,error,queued;
    uint8_t queue[512];
    resource_contact_state contact;
    uint16_t current_pcm,menu_mode,combat_delay;
    uint8_t pcm_suppression;
    uint8_t flashlight[RESOURCE_FLASHLIGHT_BYTES];
    bool contact_valid;
    /* Runtime device bindings are supplied by the owner, never serialized. */
    resource_tasks_io io;
} resource_room;
bool resource_room_contact_capture(resource_room *,unsigned file,unsigned music);
bool resource_room_contact_return(resource_room *,unsigned file);
/* Source17314/1761C/1763E, retaining the same live palette/VDP/actor owner
 * across the original15/17/18 file switch. The owner handles file task17. */
bool resource_room_file_scene(resource_room *,unsigned current_file,unsigned file,unsigned scene);
bool resource_room_begin(resource_room *room,const void *bank,size_t bytes,
    const void *sub,size_t subbytes,unsigned scene,uint32_t rng);
bool resource_room_scene(resource_room *room,unsigned scene);
bool resource_room_control(resource_room *room,unsigned actor,unsigned command);
bool resource_room_voice(resource_room *room,unsigned voice);
bool resource_room_pcm_wait(resource_room *room);
bool resource_room_suppress_actor_effects(resource_room *);
bool resource_room_pcm_command(resource_room *,unsigned command);
bool resource_room_actor_pcm_suppression(resource_room *,unsigned value);
bool resource_room_popup(resource_room *room,unsigned picture,unsigned position);
bool resource_room_tick(resource_room *room);
bool resource_room_draw(resource_room *room,uint16_t pixels[256*192]);
/* Native192x128 at x32, transparent outside. Fit once in the display adapter. */
bool resource_room_draw_native(resource_room *room,uint16_t pixels[256*192]);
/* Retain the caller's native picture between draws. Invalidate the lease
 * whenever that buffer is used for other artwork, text, filters or fades. */
bool resource_room_draw_native_cached(resource_room *room,uint16_t pixels[256*192],
    resource_background_picture *picture,bool *copied);
/* Per-play save state: pointer-free native commands/actors/VDP tile memory.
 * No rendered bitmap or captured original-game state is stored in the ROM.
 * Input resource hashes are checked before restoring into freshly bound data. */
size_t resource_room_checkpoint_bytes(resource_room *room);
bool resource_room_checkpoint_export(resource_room *room,unsigned file,bool popup,
    void *output,size_t bytes);
/* Scatter output contains exactly the same NRCS bytes as contiguous export. */
bool resource_room_checkpoint_export_blocks(resource_room *,unsigned file,bool popup,
    unsigned pcm,uint8_t *const blocks[],size_t block_bytes,size_t bytes);
bool resource_room_checkpoint_header(const void *input,size_t bytes,
    unsigned *file,unsigned *scene,bool *popup);
bool resource_room_checkpoint_import(resource_room *room,const void *input,size_t bytes);
/* Caller-owned PCM load context. Store the actual selected source bank,
 * rather than inferring it from a story flag after restoring actors. */
bool resource_room_checkpoint_with_pcm(void *input,size_t bytes,unsigned bank);
unsigned resource_room_checkpoint_pcm(const void *input,size_t bytes);

bool resource_room_draw_indices_cached(resource_room *,uint8_t out[256*192],uint16_t palette[64],resource_background_picture *,bool *);

bool resource_room_cd_begin(resource_room *,unsigned track);
bool resource_room_cd_wait(resource_room *);
void resource_room_cd_clock(resource_room *,uint32_t bcd_clock);

bool resource_room_cd_time(resource_room *,unsigned,unsigned,unsigned);

bool resource_room_flashlight_mode(resource_room *,unsigned);
bool resource_room_flashlight_input(resource_room *,unsigned,unsigned);
bool resource_room_flashlight_touch(resource_room *,int,int);
bool resource_room_menu_return(resource_room *);
