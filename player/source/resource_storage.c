/* SPDX-License-Identifier: Zlib
 * Cache construction adapted from BlocksDS filesystem_setup.c,
 * Copyright (C) 2023 Antonio Nino Diaz.
 * NitroFS is one shared mount. Reopening a voice or scene must not remount
 * the enclosing ROM and rebuild its FAT chain while other streams are live.
 * Source bytes and the original audio/video clocks are not changed. */
#include <filesystem.h>
#include <fat.h>
#include <errno.h>
#include <stdint.h>
#ifndef STORAGE_BASELINE
#include <stddef.h>
#include <stdlib.h>
/* Pinned SDK FatFs declaration; layout is checked against the linked SDK. */
#define DIR StorageFatDir
#include "storage_compat/ff.h"
#undef DIR
extern FRESULT libnds_f_lseek(FIL *file, FSIZE_t offset);
#ifdef __NDS__
_Static_assert(offsetof(FIL,cltbl)==40,"Re-audit the BlocksDS FatFs ABI");
#endif
#endif
extern bool __real_nitroFSInit(const char *basepath);
volatile int32_t resource_storage_state[6];
static bool mounted;
#ifndef STORAGE_BASELINE
static int storage_cache(FIL *file,uint32_t maximum){
    if(file->cltbl)return FAT_INIT_LOOKUP_CACHE_ALREADY_ALLOCATED;
    unsigned bytes=maximum<2048?maximum:2048;
    unsigned limit=maximum>32768?maximum:32768;
    if(limit>32768)limit=32768;
    if(bytes<8)return FAT_INIT_LOOKUP_CACHE_OUT_OF_MEMORY;
    for(unsigned attempt=0;attempt<2;attempt++){
        file->cltbl=malloc(bytes);
        if(!file->cltbl){resource_storage_state[5]=1;return FAT_INIT_LOOKUP_CACHE_OUT_OF_MEMORY;}
        file->cltbl[0]=bytes/sizeof(DWORD);
        FRESULT result=libnds_f_lseek(file,CREATE_LINKMAP);
        unsigned words=file->cltbl[0];
        if(result==FR_OK && words>=2 && words<=bytes/sizeof(DWORD)){
            DWORD *smaller=realloc(file->cltbl,words*sizeof(DWORD));
            if(smaller)file->cltbl=smaller;
            resource_storage_state[2]=0;resource_storage_state[3]=words*sizeof(DWORD);
            return 0;
        }
        /* FR_NOT_ENOUGH_CORE reports the required count, but the pinned SDK
         * leaves this incomplete table installed. Clear it BEFORE another
         * seek can use entries beyond its allocation. A failed cache is
         * optional; the ordinary FatFs reader remains valid. */
        free(file->cltbl);file->cltbl=NULL;
        resource_storage_state[2]=result;
        if(result!=FR_NOT_ENOUGH_CORE || words>limit/sizeof(DWORD) || words<=bytes/sizeof(DWORD)){
            resource_storage_state[5]=1;return -(100+(int)result);
        }
        bytes=words*sizeof(DWORD);
    }
    resource_storage_state[5]=1;return FAT_INIT_LOOKUP_CACHE_OUT_OF_MEMORY;
}
int __wrap_fatInitLookupCache(int descriptor,uint32_t maximum){
    if(((uint32_t)descriptor>>28 & 7)!=1)return FAT_INIT_LOOKUP_CACHE_NOT_SUPPORTED;
    return storage_cache((FIL *)(uintptr_t)((uint32_t)descriptor & 0x0fffffff),maximum);
}
#endif
bool __wrap_nitroFSInit(const char *basepath){
    resource_storage_state[0]++;
#ifndef STORAGE_BASELINE
    if(mounted && !basepath)return true;
#endif
#ifdef STORAGE_TEST_PATH
    if(!basepath)basepath=STORAGE_TEST_PATH;
#endif
    mounted=false;
    resource_storage_state[1]++;
    if(!__real_nitroFSInit(basepath)){resource_storage_state[4]=errno;return false;}
    mounted=true;
    return true;
}
