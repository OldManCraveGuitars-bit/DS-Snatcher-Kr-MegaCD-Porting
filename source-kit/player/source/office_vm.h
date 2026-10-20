#pragma once
#include <stdbool.h>
#include <stdint.h>
#define OFFICE_QUEUE_CAPACITY 16
#define OFFICE_STATE_BYTES 132
extern unsigned office_queue[OFFICE_QUEUE_CAPACITY], office_queue_count;
extern unsigned office_speakers[OFFICE_QUEUE_CAPACITY];
void office_reset(void);
bool office_choose(unsigned option);
void office_export(uint8_t out[OFFICE_STATE_BYTES]);
void office_import(const uint8_t in[OFFICE_STATE_BYTES]);
