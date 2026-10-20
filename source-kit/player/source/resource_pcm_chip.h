#ifndef RESOURCE_PCM_CHIP_H
#define RESOURCE_PCM_CHIP_H
#include <stdint.h>
#include <stddef.h>
/* RF5C164 bus and native-rate sample clock. The caller schedules register
 * writes, driver updates and output resampling; one tick is50MHz/1536. */
typedef struct {
    uint8_t ram[65536],registers[8][7];
    uint32_t position[8]; /*16.11 sample address, wraps at32 bits */
    uint8_t bank,channel,enabled,off;
} resource_pcm_chip;
void resource_pcm_chip_reset(resource_pcm_chip *chip);
void resource_pcm_chip_write(resource_pcm_chip *chip,unsigned reg,uint8_t value);
uint8_t resource_pcm_chip_read(const resource_pcm_chip *chip,unsigned reg);
void resource_pcm_chip_run(resource_pcm_chip *chip,int16_t *stereo,size_t frames);
#endif
