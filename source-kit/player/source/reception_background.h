#ifndef RECEPTION_BACKGROUND_H
#define RECEPTION_BACKGROUND_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "resource_opening.h"
const resource_opening_state *reception_background_video(void);
bool reception_background_random_state(uint32_t *,bool store);
extern unsigned reception_source_epoch;
bool reception_background_draw(uint16_t output[256*192]);
bool reception_background_draw_cached(uint16_t output[256*192],bool *copied);
void reception_background_invalidate_picture(void);
bool reception_background_scene(unsigned scene);
bool reception_background_source(unsigned file,unsigned scene);
bool reception_background_contact_capture(void);
bool reception_background_contact_return(unsigned *scene_key);
bool reception_background_pcm_command(unsigned command);
bool reception_background_actor_pcm_suppression(unsigned value);
bool reception_background_supports(unsigned file);
bool reception_background_supports_scene(unsigned file,unsigned scene);
bool reception_background_matches(unsigned code);
/* During original file tasks, the live room still owns the fading picture. */
bool reception_background_presenting(unsigned code);
unsigned reception_background_contact_music(void);
bool reception_background_restore_source(unsigned file,unsigned scene);
bool reception_background_control(unsigned actor,unsigned command);
bool reception_background_menu_return(void);
bool reception_background_voice(unsigned voice);
bool reception_background_pcm_wait(void);
bool reception_background_suppress_actor_effects(void);
bool reception_background_popup(unsigned picture,unsigned position);
bool reception_background_popup_end(void);
bool reception_background_popup_active(void);
bool reception_background_restore_popup(unsigned picture,unsigned position);
bool reception_background_busy(void);
bool reception_background_update(uint8_t pcm_flags);
bool reception_background_active(void);
bool reception_background_display_mode(unsigned value);
void reception_background_vblank(void);
extern unsigned reception_generation;
extern unsigned reception_source_scene;
bool reception_background_adopt_entry(void);
bool reception_background_rebind(void);
bool reception_background_restore(unsigned scene);
void reception_background_end(void);
/* Combat owns its picture; the source-loaded PCM bank stays alive. */
void reception_background_handoff_combat(void);
size_t reception_background_checkpoint_bytes(void);
bool reception_background_checkpoint_export_blocks(uint8_t *const [],size_t block_bytes,size_t bytes);
int reception_background_checkpoint_capture(uint8_t **output,size_t *bytes);
bool reception_background_checkpoint_restore(const uint8_t *input,size_t bytes);
/* Call after the temporary native snapshot has been released. */
bool reception_background_checkpoint_resume_audio(void);
bool reception_background_draw_indices(uint8_t output[256*192],uint16_t palette[64],bool *copied);
#endif

bool reception_background_gameover(uint8_t output[256*192],uint16_t [64]);

bool reception_background_combat_image(uint16_t output[256*192]);

bool reception_background_begin_return(uint8_t fields[128]);
bool reception_background_finish_return(void);

bool reception_background_practice_delay(unsigned value);
bool reception_background_practice_begin(void);
bool reception_background_native_combat_begin(unsigned stage);
bool reception_background_practice_reset_begin(void);
void reception_background_practice_run(void);
void reception_background_practice_input(unsigned held,unsigned pressed);
unsigned reception_background_practice_value(unsigned address);
bool reception_background_practice_done(void);
bool reception_background_practice_finish(uint16_t result[7]);
bool reception_background_practice_draw(uint16_t [256*192],uint8_t [256*192]);
bool reception_background_practice_indices(uint8_t [256*192],uint16_t [64],bool *);
int reception_background_practice_touch(int x,int y);

bool reception_background_fullframe(void);
unsigned reception_background_picture_rows(void);
bool reception_background_terminal(void);

bool reception_background_practice_draw_filtered(uint16_t [256*192],uint8_t [256*192],bool,bool,bool *);

bool reception_background_cd_begin(unsigned);
bool reception_background_cd_wait(void);

bool reception_background_cd_time(unsigned,unsigned,unsigned);

bool reception_background_cinematic(unsigned *x,unsigned *width,unsigned *rows);

bool reception_background_flashlight_mode(unsigned);
bool reception_background_flashlight_active(void);
bool reception_background_flashlight_target(unsigned *);
bool reception_background_flashlight_touch(int,int,bool);
bool reception_background_flashlight_position(int *,int *);
