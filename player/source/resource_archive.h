#ifndef RESOURCE_ARCHIVE_H
#define RESOURCE_ARCHIVE_H
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
/* Read one original bank without loading the complete archive into DS RAM.
 * The caller frees the returned allocation. No original payload is in code. */
uint8_t *resource_archive_load(FILE *file,const char *name,size_t *bytes);
#endif
