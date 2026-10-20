#ifndef RESOURCE_CONTACT_H
#define RESOURCE_CONTACT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Original SUB14F70 compact caller context at9C18. This is not a saved
 * bitmap or emulator state. SP18 custom11 saves/restores these source flags. */
typedef struct { uint8_t bytes[72]; } resource_contact_state;
typedef struct {
    uint16_t scene,pcm,display_mode,menu_mode;
    uint8_t music,pcm_suppression,actor_pcm_suppression;
} resource_contact_context;

bool resource_contact_capture(resource_contact_state *,const resource_contact_context *,
    const uint8_t actors[64*80]);
/* Emit original15/17/13/10/32/7 packets and update the original control
 * context. The caller must execute every packet and original completion wait.
 * Return excludes the trailing zero task. Failure leaves outputs unchanged. */
size_t resource_contact_return(const resource_contact_state *,resource_contact_context *,
    uint8_t *queue,size_t capacity);
/* Source1507A applies the saved64 actor flags after task32 reloads the scene. */
bool resource_contact_restore_actors(const resource_contact_state *,uint8_t actors[64*80]);
#endif
