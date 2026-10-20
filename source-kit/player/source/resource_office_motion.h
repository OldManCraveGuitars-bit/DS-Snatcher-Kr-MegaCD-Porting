#ifndef RESOURCE_OFFICE_MOTION_H
#define RESOURCE_OFFICE_MOTION_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
bool resource_office_bind(const void *data, size_t size);
bool resource_office_begin(unsigned voice_index, uint16_t poses[3]);
bool resource_office_update(unsigned samples, unsigned sample_rate, uint16_t poses[3]);
#endif
