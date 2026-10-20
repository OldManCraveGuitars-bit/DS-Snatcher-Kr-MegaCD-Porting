#ifndef RESOURCE_ACTOR_H
#define RESOURCE_ACTOR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Native SUBCODE F7F0/F8C0 actor records, 64 actors of 80 big-endian bytes.
 * No frame recording or precomputed motion timeline is an input.
 * Facial sequences and native related-actor position/velocity controls.
 * Graphics/palette/sound
 * job opcodes deliberately return false until their engines are implemented. */
typedef struct {
    uint32_t rng;
    uint32_t pcm_clock;
    uint16_t iterations;
    uint8_t freeze;
    uint8_t pcm_flags;
    uint32_t commands;
    uint16_t error_actor;
    uint8_t error_command;
    uint8_t error;
} resource_actor_clock;

uint16_t resource_actor_random(uint32_t *state);
/* Native FDD2 task controls: resume/pause/hide/wait/retain/restart. A wait
 * finishes only on actor deletion or its own98 completion signal. */
bool resource_actor_control(uint8_t actors[64*80],unsigned actor,unsigned operation,bool *complete);
bool resource_actor_tick(uint8_t actors[64 * 80], const void *resource,
    size_t resource_size, uint32_t resource_base, resource_actor_clock *clock,
    uint64_t actor_mask);
typedef struct {
    void *context;
    bool (*command)(void *context,unsigned actor,unsigned opcode,unsigned arg,uint16_t value,uint32_t sequence);
    bool palette_blocked;
    const uint8_t *(*resolve)(void *context,uint32_t address,size_t bytes);
} resource_actor_effects;
bool resource_actor_tick_effects(uint8_t actors[64*80],const void *resource,size_t resource_size,
    uint32_t resource_base,resource_actor_clock *clock,uint64_t actor_mask,resource_actor_effects *effects);
/* Optional native7868 iteration register; retains the original clock ABI. */
bool resource_actor_tick_registers(uint8_t actors[64*80],const void *resource,size_t resource_size,
    uint32_t resource_base,resource_actor_clock *clock,uint64_t actor_mask,
    resource_actor_effects *effects,uint16_t *iteration_register);
#endif
