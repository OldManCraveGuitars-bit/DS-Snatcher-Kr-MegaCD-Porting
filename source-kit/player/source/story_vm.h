#pragma once
#include <stdint.h>
#include <stdbool.h>
enum { STORY_ERROR=-1,STORY_MENU=1,STORY_DIALOG,STORY_VOICE,STORY_WAIT,STORY_GRAPHICS,STORY_SCENE,STORY_AUDIO_WAIT,STORY_MEDIA_WAIT,STORY_BATTLE,STORY_GAME_OVER,STORY_KEYBOARD,STORY_BACKUP_QUERY,STORY_BACKUP_SAVE,STORY_END,STORY_INPUT_WAIT,STORY_CD_TIME,STORY_CREDITS,STORY_BACKUP_CONTEXT };
int story_submit_name(const char *name);
/* NAME_INPUT_* from name_keyboard.h, valid while STORY_KEYBOARD is active. */
unsigned story_keyboard_kind(void);
#define STORY_MENU_CAPACITY 16
#define STORY_STATE_BYTES 768
extern unsigned story_menu_count,story_menu_text[STORY_MENU_CAPACITY];
extern unsigned story_text,story_speaker,story_event_arg,story_event_op,story_generation;
extern unsigned story_event_arg2,story_event_arg3;
extern unsigned story_event_pc;
extern bool story_menu_enabled[STORY_MENU_CAPACITY];
void story_reset(void);
int story_start(void);
/* Actual normal chapter-entry fields/RNG, independent of manual saves. */
void story_set_chapter_recorder(void (*record)(unsigned,const uint8_t[360]));
int story_replay_chapter(unsigned chapter,const uint8_t seed[360]);
int story_start_after_entry(void);
int story_after_native_flight(void);
int story_after_native_forecourt(void);
int story_advance(void);
int story_pick(unsigned option);
int story_cancel(void);
unsigned story_read_field(unsigned key);
void story_export(uint8_t out[STORY_STATE_BYTES]);
void story_export_save(uint8_t out[STORY_STATE_BYTES]);
void story_save_committed(void);
unsigned story_save_counter(void);
uint32_t story_random_state(void);
void story_set_random_provider(bool (*provider)(uint32_t *,bool));
void story_set_random_state(uint32_t value);
bool story_blob_valid(const uint8_t in[STORY_STATE_BYTES]);
bool story_import(const uint8_t in[STORY_STATE_BYTES]);
int story_blob_text(const uint8_t in[STORY_STATE_BYTES]);
void story_export_fields(uint8_t out[132]);
int story_legacy_dialog(unsigned line);
int story_start_at(unsigned pc,const uint8_t fields[132]);
unsigned story_text_offset(void);
unsigned story_module(void);
/* Last original0A request, orFFFF for old checkpoints that did not retain it. */
unsigned story_music_request(void);
/* Synchronize save metadata after original custom11 restores caller music. */
bool story_set_music_request(unsigned command);
/* Presentation checkpoint metadata; old blobs retain unknownFFFF. */
unsigned story_display_under(void);
void story_set_display_under(unsigned scene);
bool story_battle_result(unsigned damage);
/* Includes the original790C decision result for encounter14. */
bool story_native_battle_result(const uint16_t values[7],unsigned damage);

uint8_t *story_native_fields(void);
int story_after_native_return(void);

/* Source7908/0A/0C/0E/14/16/18 in that order, after native practice completes. */
bool story_practice_result(const uint16_t values[7]);
bool story_practice_result_with_damage(const uint16_t values[7],unsigned damage);

#define STORY_FIELD_BYTES 352u
void story_backup_storage(unsigned result,unsigned occupied_mask);
bool story_backup_preview(uint8_t out[STORY_STATE_BYTES]);
bool story_backup_finish(unsigned result);
unsigned story_backup_word(void);

void story_bind_flashlight(bool (*query)(unsigned *));
bool story_flashlight_menu(void);
