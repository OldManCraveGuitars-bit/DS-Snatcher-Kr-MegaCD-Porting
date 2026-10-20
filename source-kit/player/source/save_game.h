#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "office_vm.h"
#include "story_vm.h"
typedef struct {
    uint8_t state[OFFICE_STATE_BYTES];
    uint16_t queue[OFFICE_QUEUE_CAPACITY],speakers[OFFICE_QUEUE_CAPACITY];
    uint16_t count,mode,line,page;
    uint8_t story[STORY_STATE_BYTES];
} GameCheckpoint;
enum { SAVE_OK=0, SAVE_MISSING=1, SAVE_NO_SD=2, SAVE_IO_ERROR=3, SAVE_INVALID=4 };
void save_init(void);
/* Keep live audio supplied between synchronous SD operations. No save calls
 * may be made by this optional service callback. Legacy core bytes stay unchanged. */
void save_set_io_service(void (*service)(void));
/* Optional version4 extension for the native room's per-play state. The
 * capture callback transfers malloc ownership, or returns0 for no native
 * room/-1 on failure. No pointers or rendered frames belong in this data.
 * Older224/992-byte slots remain readable; the GameCheckpoint ABI is kept. */
void save_set_native_io(int (*capture)(uint8_t **output,size_t *bytes),
    bool (*restore)(const uint8_t *input,size_t bytes));
/* Fixed small allocations avoid requiring one contiguous 173KiB hole.
 * Capture transfers ownership of every non-NULL block, even on failure. */
/* The Act2 disc bank leaves smaller gaps. Keep the full256KiB limit
 * and identical on-disk state while fitting those gaps with2KiB blocks. */
#define SAVE_NATIVE_BLOCK_BYTES 2048u
#define SAVE_NATIVE_BLOCKS 128u
typedef struct {size_t bytes;uint8_t *blocks[SAVE_NATIVE_BLOCKS];} SaveNativeBuffer;
bool save_native_buffer_allocate(SaveNativeBuffer *,size_t bytes);
void save_set_native_buffer_io(int (*capture)(SaveNativeBuffer *),
    bool (*restore)(const uint8_t *input,size_t bytes));
int save_restore_native(void);
#define SAVE_SLOT_COUNT 5u
unsigned save_current_slot(void);
bool save_select_slot(unsigned slot);
int save_peek_slot(unsigned slot,GameCheckpoint *out);
int save_load(GameCheckpoint *out);
int save_write(const GameCheckpoint *state);
bool checkpoint_valid(const GameCheckpoint *state);
