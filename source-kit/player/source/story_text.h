#ifndef STORY_TEXT_H
#define STORY_TEXT_H
#include <stdint.h>
/* Main-thread only. Pointers remain valid until sixteen distinct page misses.
 * UI callers consume each string immediately; no save contains a pointer. */
const uint16_t *story_text_page(unsigned index);
extern volatile unsigned story_text_fault,story_text_reads,story_text_hits;
#endif
