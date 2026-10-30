#ifndef RESOURCE_ENTRY_H
#define RESOURCE_ENTRY_H
#include "resource_opening.h"
#include "resource_opening_presentation.h"
#include "resource_program.h"
#include "resource_pcm_driver.h"
#include "resource_pcm_output.h"

/* Input ownership stays with the host. Scene views must remain valid until
 * the next file request; PCM sector reads are completed before busy clears. */
typedef struct {
    void *context;
    const uint8_t *(*file)(void *context,unsigned id,size_t *bytes);
    bool (*pcm)(void *context,unsigned sector,unsigned sectors,void *destination);
    bool (*music)(void *context,unsigned command);
} resource_entry_io;
/* A real CD stream supplies status and BCD position; the scene interpreter
 * never substitutes a recorded duration or a guessed seek timer. */
typedef struct {
    void *context;
    bool (*start)(void *context,unsigned track);
    bool (*pause)(void *context);
    int16_t (*status)(void *context);
    uint32_t (*clock)(void *context);
    bool (*mix)(void *context,int16_t *stereo,unsigned frames);
} resource_entry_cd;
/* Optional shared sound owner. The entry retains scene/task ownership,
 * while the external renderer ticks and renders the single PCM device. */
typedef struct {
    void *context;
    resource_pcm_driver *pcm;
    bool (*render)(void *,int16_t *,unsigned);
    bool (*command)(void *,unsigned,unsigned);
    void (*status)(void *,uint8_t *,uint8_t *);
    void (*event)(void *,unsigned,unsigned,unsigned);
} resource_entry_audio;
typedef struct {
    resource_opening_state video;
    resource_opening_presentation presentation;
    resource_program_state program;
    resource_tasks_state tasks;
    resource_scene_devices devices;
    resource_pcm_driver *owned_pcm;
    resource_pcm_output output;
    resource_entry_io io;
    uint8_t transfer[2048];
    uint32_t samples,next_tick,voice_begin,voice_end,external_done;
    unsigned voice_id;
    bool external_pending;
    unsigned file,scene,pcm_load,music_pending,error;
    uint16_t handoff;
    uint8_t tick_phase;
    bool finished,initialized;
    resource_entry_cd cd;
    resource_entry_audio audio;
} resource_entry_state;
void resource_entry_end(resource_entry_state *);
bool resource_entry_begin_at_audio(resource_entry_state *,const void *,size_t,const void *,size_t,
    const resource_pcm_effect_bank *,const resource_entry_io *,uint32_t,unsigned,uint16_t,const resource_entry_audio *);
bool resource_entry_set_audio(resource_entry_state *,const resource_entry_audio *);
resource_pcm_driver *resource_entry_audio_pcm(resource_entry_state *);
bool resource_entry_audio_tail(resource_entry_state *,int16_t *,unsigned);
bool resource_entry_begin(resource_entry_state *state,const void *script,size_t script_bytes,
    const void *subcode,size_t subcode_bytes,const resource_pcm_effect_bank *pcm,
    const resource_entry_io *io,uint32_t rng);
bool resource_entry_begin_at(resource_entry_state *state,const void *script,size_t script_bytes,
    const void *subcode,size_t subcode_bytes,const resource_pcm_effect_bank *pcm,
    const resource_entry_io *io,uint32_t rng,unsigned file,uint16_t pc);
bool resource_entry_continue(resource_entry_state *state,uint16_t pc);
/* StoryVM may submit a native task only at a yielded dialogue boundary.
 * It never resets the source actors, sound devices or shared sample clock. */
bool resource_entry_pcm(resource_entry_state *state,unsigned voice);
bool resource_entry_control(resource_entry_state *state,unsigned actor,unsigned command);
bool resource_entry_popup(resource_entry_state *state,unsigned picture,unsigned position);
/* Switch a completed original module without resetting actors, palettes,
 * PCM, RNG or the shared source/audio clock. Script ownership stays external. */
bool resource_entry_switch_program(resource_entry_state *state,const void *script,size_t bytes,uint16_t pc);
void resource_entry_set_cd(resource_entry_state *state,const resource_entry_cd *cd);
/* Render audio and run original script/actors/PCM on the same source clock.
 * 0=running, 1=dialogue handoff, 2=unfulfilled music request, -1=error.
 * A missing music backend stalls explicitly; it never consumes the command. */
int resource_entry_render(resource_entry_state *state,int16_t *stereo,unsigned frames);
/* While StoryVM owns a dialogue/menu boundary, continue original actors,
 * palette/scroll jobs and sound devices without consuming more script. */
int resource_entry_idle_render(resource_entry_state *state,int16_t *stereo,unsigned frames);
bool resource_entry_draw(resource_entry_state *state,uint16_t output[256*192]);

bool resource_entry_pcm_effects_blocked(resource_entry_state *,bool);

/* Adopt a completed live room without resetting its actors, palettes or RNG.
 * The caller retains the room until this returns and retains/rebinds original
 * bank/subcode memory for the entry lifetime. No shared audio state is reset. */
#include "resource_room.h"
/* Allocate live rooms in this storage when they may become a native entry.
 * The owner switch preserves the shared VDP prefix in place. */
typedef union {resource_room room;resource_entry_state entry;} resource_entry_room_storage;
bool resource_entry_take_room(resource_entry_room_storage *,const void *script,size_t bytes,
    const resource_entry_io *,unsigned file,uint16_t pc,uint8_t fields[128],const resource_entry_audio *);
bool resource_entry_adopt_room(resource_entry_state *,const resource_room *,
    const void *script,size_t bytes,const resource_entry_io *,unsigned file,
    uint16_t pc,uint8_t fields[128],const resource_entry_audio *);
/* Only a completed source tick is transferable back to the user-paced VM. */
bool resource_entry_export_room(const resource_entry_state *,resource_room *);
/* Release shared-audio entries into their original union allocation. Failure
 * preserves every byte. The caller takes the storage and its data references. */
bool resource_entry_release_room(resource_entry_room_storage *);
bool resource_entry_yield_menu(resource_entry_state *,uint16_t);
#endif
