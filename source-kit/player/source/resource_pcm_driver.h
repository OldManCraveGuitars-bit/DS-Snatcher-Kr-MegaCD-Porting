#ifndef RESOURCE_PCM_DRIVER_H
#define RESOURCE_PCM_DRIVER_H
#include "resource_pcm_effect.h"
#include "resource_pcm_chip.h"
typedef struct {
    resource_pcm_effect_bank bank;
    resource_pcm_chip chip;
    uint8_t ram[2048]; /* Original driver state7F800..7FFFF */
    uint8_t storage[106496]; /*65000..7EFFF, source-sector load destination */
    uint16_t load_id; /*15120 */
    uint8_t flags,attenuation,error; /*1511E,1512A */
} resource_pcm_driver;
bool resource_pcm_driver_bind(resource_pcm_driver *driver,const resource_pcm_effect_bank *bank);
/* Original154F8 request. Pending sector/destination fields are observable
 * in ram28/2C/30; the caller supplies actual file data with complete_load.
 * Until then the native pending/busy bits remain set. */
bool resource_pcm_driver_load(resource_pcm_driver *driver,unsigned id);
bool resource_pcm_driver_complete_load(resource_pcm_driver *driver,const void *data,size_t bytes);
/* Supply whole 2048-byte sectors, retaining busy until every requested
 * sector arrives. This avoids a second full-bank allocation on the DS. */
bool resource_pcm_driver_feed_load(resource_pcm_driver *driver,const void *data,size_t bytes);
bool resource_pcm_driver_effect(resource_pcm_driver *driver,unsigned command,unsigned pair);
/* Original16128 uploads the resident drum waves into PCM RAM8000..FCFF.
 * Original160BA consumes the three packed Main-CPU bytes only when their
 * sequence byte changes. Drum voices use PCM channel pairs2 and3. */
bool resource_pcm_driver_drum_bank(resource_pcm_driver *driver);
bool resource_pcm_driver_drums(resource_pcm_driver *driver,uint8_t first,uint8_t second,uint8_t levels,uint8_t sequence);
/* PCM cursor/refill/start/on-off service. FM command/duck dispatch and CD
 * transfer timing are separate devices, not silently assumed complete. */
bool resource_pcm_driver_tick(resource_pcm_driver *driver);
void resource_pcm_driver_music_level(resource_pcm_driver *driver,unsigned level);
#endif
