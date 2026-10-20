#ifndef NATIVE_ENTRY_H
#define NATIVE_ENTRY_H
#include <stdbool.h>
#include <stdint.h>
#include "resource_entry.h"
#include "resource_room.h"
#include "resource_portrait_history.h"
/* The live source786A RNG belongs to this native engine until its room
 * transfers. Runtime callbacks are not included in snapshots or resources. */
bool native_entry_random_state(uint32_t *,bool store);
void native_entry_set_random_io(uint32_t (*seed)(void),void (*retire)(uint32_t));
const resource_portrait_snapshot *native_entry_portraits(unsigned played_samples);
bool native_entry_copy_room(resource_room *room);
/* Stop the presenter and release consumed history before transferring.
 * Complete music_player_entry_release before servicing any audio callback. */
bool native_entry_take_initial_room(resource_room **,uint8_t **,uint8_t **);
bool native_entry_initial_transferred(void);
/* Caller must stop the presentation IRQ after copying the last picture. */
bool native_entry_release_played_history(void);
#define NATIVE_ENTRY_BUFFER_FRAMES 12288
#define NATIVE_FLIGHT_BUFFER_FRAMES 12288
/* The DAC ring already holds12288 frames. An additional16384-frame
 * reserve leaves room for original portrait/VDP allocations at voice170.
 * A larger speculative queue can exhaust the ordinary4MB DS heap. */
#define NATIVE_RETURN_RESERVE_FRAMES 16384
#define NATIVE_RETURN_BUFFER_FRAMES (NATIVE_FLIGHT_BUFFER_FRAMES+NATIVE_RETURN_RESERVE_FRAMES)
#define NATIVE_ENTRY_FRAME_WIDTH 192
/* DS stream adapter for the original SP06 entry. Caller owns Maxmod and
 * consumes pending music through a real backend before accepting handoff.
 * Disable its presentation IRQ before calling native_entry_end. */
bool native_entry_begin(bool (*music)(unsigned command));
bool native_entry_begin_audio(bool (*music)(unsigned),const resource_entry_audio *);
/* Explicit opening skip starts at the original ACT1 task block. */
bool native_entry_begin_title_audio(bool (*music)(unsigned),const resource_entry_audio *);
bool native_entry_flight_begin(bool (*music)(unsigned command),const resource_entry_cd *cd);
bool native_entry_flight_begin_audio(bool (*)(unsigned),const resource_entry_cd *,const resource_entry_audio *);
bool native_entry_forecourt_continue(void);
bool native_entry_forecourt_begin(bool (*music)(unsigned command),const resource_entry_cd *cd);
bool native_entry_forecourt_begin_audio(bool (*)(unsigned),const resource_entry_cd *,const resource_entry_audio *);
void native_entry_end(void);
unsigned native_entry_audio(int16_t *out,unsigned frames);
/* RGB handoff: native picture at x32 in a256-column buffer; transparent margins. */
bool native_entry_draw(unsigned played_samples,uint16_t out[256*192]);
/* Original192-column palette-index rows; height is returned separately by
 * native_entry_picture_height(sample). Keep native rows in VRAM and scale
 * once with the display affine transform. Valid until the next audio fill;
 * copy to the inactive VRAM page, then publish palette/page together. */
bool native_entry_frame(unsigned played_samples,const uint8_t **pixels,const uint16_t **palette,unsigned *sample);
bool native_entry_frame_view(unsigned,const uint8_t **,const uint8_t *const **,const uint16_t **,unsigned *);
/* frame_view rows are immutable records; expand compressed return rows into
 * caller-owned aligned scratch, or return the unchanged raw row directly. */
const uint8_t *native_entry_row_pixels(const uint8_t *,uint8_t scratch[192]);
/* Nonzero identity remains valid through sharing, never across allocation
 * reuse. Zero requires an unconditional upload. */
unsigned native_entry_row_serial(const uint8_t *);
unsigned native_entry_picture_height(unsigned sample);
/* Zero disables reuse for legacy uncompressed flight frames. */
unsigned native_entry_pixel_revision(unsigned sample);
int native_entry_status(void);
unsigned native_entry_music(void);
unsigned native_entry_handoff(void);
unsigned native_entry_written(void);
/* Fixed sample boundary, unlike written which advances during user waits. */
unsigned native_entry_handoff_sample(void);
unsigned native_entry_current_sample(void);
unsigned native_entry_voice_begin(void);
unsigned native_entry_voice_end(void);
bool native_entry_external_voice_ready(void);
bool native_entry_prepare_voice(unsigned voice);
bool native_entry_start_voice(unsigned voice);
/* Pending/playing=1, completed=0, invalid/error=-1. Captions use source PCM samples. */
int native_entry_story_voice(unsigned played_samples,unsigned *samples,bool *audible);
void native_entry_story_voice_release(void);
#endif

bool native_entry_pcm_effects_blocked(bool);

/* On success, takes the room storage and its bank/subcode allocations.
 * On failure they remain owned by the caller, unchanged. */
bool native_entry_return_begin(resource_room *,uint8_t fields[128],const volatile unsigned *,
    const resource_entry_audio *,const resource_entry_cd *);
unsigned native_entry_return_voice(unsigned);
bool native_entry_return_rebind(void);
bool native_entry_return_room(resource_room *);
/* After presenter/history release, move all three owned allocations together. */
bool native_entry_take_return_room(resource_room **,uint8_t **,uint8_t **);
bool native_entry_return_transferred(void);
bool native_entry_is_return(void);
void native_entry_return_retire(unsigned played);
bool native_entry_return_reserve_room(void);

bool native_entry_return_ready(void);
bool native_entry_return_release_history(void);

bool native_entry_return_resources(uint8_t **,uint8_t **);
/* Called after frame_view has selected/replayed this presentation. */
unsigned native_entry_contiguous_row_serial(unsigned sample,unsigned row);
