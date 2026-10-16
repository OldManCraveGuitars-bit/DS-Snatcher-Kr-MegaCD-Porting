#include <stdio.h>
extern size_t audio_serviced_fread(void *,size_t,size_t,FILE *);
#include "resource_archive.h"
#include <stdlib.h>
#include <string.h>
static uint16_t word(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t longword(const uint8_t *p){return word(p)|(uint32_t)word(p+2)<<16;}
uint8_t *resource_archive_load(FILE *f,const char *name,size_t *bytes){
    if(bytes)*bytes=0;
    if(!f || !name || !bytes || !name[0] || strlen(name)>=16 || fseek(f,0,SEEK_END))return NULL;
    long length=ftell(f);if(length<8 || length>32*1024*1024 || fseek(f,0,SEEK_SET))return NULL;
    uint8_t header[8];if(fread(header,1,8,f)!=8 || memcmp(header,"NRS1",4) || word(header+4)!=1)return NULL;
    unsigned count=word(header+6);if(!count || count>256 || 8+count*56>(unsigned)length)return NULL;
    for(unsigned i=0;i<count;i++){
        uint8_t row[56];if(fread(row,1,56,f)!=56 || ferror(f))return NULL;
        if(!memchr(row,0,16))return NULL;
        uint32_t at=longword(row+16),n=longword(row+20);
        if(at<8+count*56 || at>(unsigned)length || n>(unsigned)length-at)return NULL;
        if(strcmp((const char *)row,name))continue;
        if(!n || n>1024*1024 || fseek(f,at,SEEK_SET))return NULL;
        uint8_t *data=malloc(n);if(!data)return NULL;
        if(audio_serviced_fread(data,1,n,f)!=n || ferror(f)){free(data);return NULL;}
        *bytes=n;return data;
    }
    return NULL;
}
