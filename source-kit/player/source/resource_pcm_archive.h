#ifndef RESOURCE_PCM_ARCHIVE_H
#define RESOURCE_PCM_ARCHIVE_H
#include "resource_pcm_driver.h"
#include <stdio.h>
bool resource_pcm_archive_load(resource_pcm_driver *,FILE *,const char *,unsigned);
#endif
