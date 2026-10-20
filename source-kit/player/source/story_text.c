#include "story_text.h"
#include "../generated/story_text_archive.h"
#include "resource_checksum.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <filesystem.h>
extern size_t audio_serviced_fread(void *,size_t,size_t,FILE *);
#ifndef STORY_TEXT_PATH
#define STORY_TEXT_PATH "nitro:/story-text-r22.stp"
#endif
#define CACHE_PAGES 16
typedef struct {unsigned id;bool valid;uint16_t glyphs[STORY_TEXT_PAGE_UNITS];} text_page;
static text_page cache[CACHE_PAGES];
static unsigned victim;
static FILE *file;
static unsigned file_bytes;
volatile unsigned story_text_fault,story_text_reads,story_text_hits;
static unsigned word(const unsigned char *p){return p[0]|(unsigned)p[1]<<8;}
static unsigned longword(const unsigned char *p){return word(p)|word(p+2)<<16;}
static bool exact(void *p,unsigned bytes){return audio_serviced_fread(p,1,bytes,file)==bytes && !ferror(file);}
static bool begin(void){
    unsigned char header[12];
    if(!nitroFSInit(NULL) || !(file=fopen(STORY_TEXT_PATH,"rb")))return false;
    if(fseek(file,0,SEEK_END))return false;
    long size=ftell(file);
    if(size<12 || (unsigned long)size>UINT_MAX || fseek(file,0,SEEK_SET))return false;
    file_bytes=(unsigned)size;
    return exact(header,12) && !memcmp(header,"STP1",4) && word(header+4)==1 &&
        word(header+6)==STORY_TEXT_PAGE_UNITS && longword(header+8)==STORY_TEXT_PAGE_COUNT &&
        file_bytes>=12+STORY_TEXT_PAGE_COUNT*12;
}
const uint16_t *story_text_page(unsigned index){
    if(story_text_fault)return NULL;
    if(index>=STORY_TEXT_PAGE_COUNT){story_text_fault=1;return NULL;}
    for(unsigned i=0;i<CACHE_PAGES;i++)if(cache[i].valid && cache[i].id==index){
        story_text_hits++;return cache[i].glyphs;
    }
    if(!file && !begin()){
        if(file)fclose(file);file=NULL;story_text_fault=2;return NULL;
    }
    unsigned char entry[12];
    if(fseek(file,12+index*12,SEEK_SET) || !exact(entry,12)){story_text_fault=3;return NULL;}
    unsigned at=longword(entry),units=word(entry+4),crc=longword(entry+8);
    if(word(entry+6) || !units || units>STORY_TEXT_PAGE_UNITS ||
       at<12+STORY_TEXT_PAGE_COUNT*12 || at>file_bytes || units*2>file_bytes-at){
        story_text_fault=4;return NULL;
    }
    text_page *page=&cache[victim];page->valid=false;
    if(fseek(file,at,SEEK_SET) || !exact(page->glyphs,units*2)){story_text_fault=3;return NULL;}
    if(resource_crc32(page->glyphs,units*2)!=crc || page->glyphs[units-1]!=65535){story_text_fault=5;return NULL;}
    for(unsigned i=0;i+1<units;i++)if(page->glyphs[i]>=STORY_TEXT_GLYPHS && page->glyphs[i]!=65534){
        story_text_fault=5;return NULL;
    }
    page->id=index;page->valid=true;victim=(victim+1)%CACHE_PAGES;story_text_reads++;
    return page->glyphs;
}
