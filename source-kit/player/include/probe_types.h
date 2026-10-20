#ifndef PROBE_TYPES_H
#define PROBE_TYPES_H
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef char probe_int_is_32_bit[(sizeof(s32)==4)?1:-1];
#endif
