#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Lossless native192-pixel row. The raw escape preserves any unexpected
 * byte too; normal opaque six-bit VDP indices use packed or run storage. */
#define RESOURCE_PICTURE_ROW_WIDTH 192
#define RESOURCE_PICTURE_ROW_MAX 193
unsigned resource_picture_row_pack(const uint8_t input[192],uint8_t output[193]);
bool resource_picture_row_unpack(const uint8_t *input,unsigned bytes,uint8_t output[192]);
