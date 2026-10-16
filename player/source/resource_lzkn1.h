#ifndef RESOURCE_LZKN1_H
#define RESOURCE_LZKN1_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool resource_lzkn1(const uint8_t *data,size_t size,uint8_t *output,size_t capacity,size_t *decoded,size_t *consumed);
#endif
