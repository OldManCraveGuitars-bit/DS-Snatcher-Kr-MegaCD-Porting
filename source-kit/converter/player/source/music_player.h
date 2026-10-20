#ifndef MUSIC_PLAYER_H
#define MUSIC_PLAYER_H
#include <stdbool.h>
bool music_player_init(void);
bool music_player_entry_begin(void);
bool music_player_flight_begin(void);
bool music_player_forecourt_begin(void);
/* Stop entry rendering while retaining the same stream and music cursor. */
bool music_player_entry_release(void);
void music_player_end(void);
void music_player_update(void);
/* Preserve a safety reserve without draining every pending refill at each read. */
void music_player_service_io(void);
bool music_player_update_slice(void);
bool music_player_slice_active(void);
bool music_player_command(unsigned command);
bool music_player_effect(unsigned command);
bool music_player_effect_wait_begin(void);
bool music_player_music_wait_begin(void);
int music_player_sound_wait_status(void);
bool music_player_combat_begin(unsigned stage);
bool music_player_combat_effects(unsigned events);
/* Mode1 is an encoded effect-bank load, not a spoken dialogue clip.
 * Return1 for a completed bank load,0 for speech,-1 on an input error. */
int music_player_room_pcm_load(unsigned id);
bool music_player_room_pcm_effect(unsigned command,unsigned pair);
unsigned music_player_room_pcm_flags(void);
unsigned music_player_room_pcm_bank(void);
unsigned music_player_room_pcm_id(void);
unsigned music_player_room_pcm_attenuation(void);
bool music_player_room_pcm_set_attenuation(unsigned value);
void music_player_room_pcm_end(void);
unsigned music_player_played(void);
/* Original CD20 playback clock, never the look-ahead decode position. */
bool music_player_flight_cdda_sample(unsigned *sample);
extern volatile unsigned music_written,music_played,music_error,music_command,music_missing;
extern volatile unsigned music_loops,music_start_sample;
extern volatile int music_min_lead;
#endif

void music_player_service_io(void);

#include "resource_room.h"
bool music_player_return_begin(resource_room *,uint8_t fields[128]);
bool music_player_return_release(void);
bool music_player_return_reserve(void);
