#include "chapter_progress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#ifdef __NDS__
#include <fat.h>
#elif defined(_WIN32)
#include <direct.h>
#endif

enum { RECORDS=2,CRC_AT=16+RECORDS*CHAPTER_SEED_BYTES,FILE_BYTES=CRC_AT+8 };
static uint8_t seeds[RECORDS][CHAPTER_SEED_BYTES];
static uint8_t present,cleared;
static unsigned sequence,bank;
static char directory[128];
static uint32_t crc(const uint8_t *p,unsigned n){uint32_t v=~0u;while(n--){v^=*p++;for(unsigned i=0;i<8;i++)v=(v>>1)^((0u-(v&1))&0xedb88320u);}return ~v;}
static unsigned get32(const uint8_t *p){return p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static void put32(uint8_t *p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void path(char out[160],unsigned slot){snprintf(out,160,"%s/chapters-%c.bin",directory,slot?'b':'a');}
static bool seed_valid(unsigned chapter,const uint8_t *p){return chapter>=2 && chapter<=3 && p && p[0]==1 && p[1]==chapter && (p[2]&15)<=9 && (p[2]>>4)<=9 && !p[3];}
static bool read_bank(unsigned slot,uint8_t data[FILE_BYTES]){
    char file[160];path(file,slot);FILE *f=fopen(file,"rb");if(!f)return false;
    size_t n=fread(data,1,FILE_BYTES,f);int tail=fgetc(f);bool good=!ferror(f);if(fclose(f))good=false;
    if(!good || n!=FILE_BYTES || tail!=EOF || memcmp(data,"SNDSACT1",8) || !get32(data+8) ||
       data[12]>1 || data[13]>3 || data[14] || data[15] || get32(data+CRC_AT)!=crc(data,CRC_AT) || get32(data+CRC_AT+4))return false;
    for(unsigned i=0;i<RECORDS;i++){
        const uint8_t *p=data+16+i*CHAPTER_SEED_BYTES;
        if(data[13]&(1u<<i)){if(!seed_valid(i+2,p))return false;}
        else for(unsigned j=0;j<CHAPTER_SEED_BYTES;j++)if(p[j])return false;
    }
    return true;
}
void chapter_progress_load(void){
    memset(seeds,0,sizeof(seeds));present=cleared=sequence=0;bank=1;directory[0]=0;
#ifdef __NDS__
    const char *drive=fatGetDefaultDrive();if(drive)snprintf(directory,sizeof(directory),"%ssnatcher_ds",drive);
#else
    const char *dir=getenv("SNATCHER_TEST_SAVE_DIR");if(dir)snprintf(directory,sizeof(directory),"%s",dir);
#endif
    if(!directory[0])return;
    uint8_t data[FILE_BYTES];
    for(unsigned i=0;i<2;i++)if(read_bank(i,data) && get32(data+8)>sequence){
        sequence=get32(data+8);bank=i;cleared=data[12];present=data[13];memcpy(seeds,data+16,sizeof(seeds));
    }
}
static bool store(void){
    if(!directory[0] || sequence==UINT_MAX)return false;
#if defined(_WIN32) && !defined(__NDS__)
    _mkdir(directory);
#else
    mkdir(directory,0777);
#endif
    uint8_t data[FILE_BYTES]={0},verified[FILE_BYTES];
    memcpy(data,"SNDSACT1",8);put32(data+8,sequence+1);data[12]=cleared;data[13]=present;
    memcpy(data+16,seeds,sizeof(seeds));put32(data+CRC_AT,crc(data,CRC_AT));
    char file[160];path(file,!bank);FILE *f=fopen(file,"wb");if(!f)return false;
    bool good=fwrite(data,1,sizeof(data),f)==sizeof(data);if(fflush(f))good=false;if(fclose(f))good=false;
    if(!good || !read_bank(!bank,verified) || memcmp(verified,data,sizeof(data)))return false;
    sequence++;bank=!bank;return true;
}
bool chapter_progress_cleared(void){return cleared!=0;}
bool chapter_progress_available(unsigned chapter){return cleared && (chapter==1 || (chapter>=2 && chapter<=3 && (present&(1u<<(chapter-2)))));}
bool chapter_progress_record(unsigned chapter,const uint8_t seed[CHAPTER_SEED_BYTES]){
    if(!seed_valid(chapter,seed))return false;
    unsigned i=chapter-2;
    /* Retain a pending entrance in RAM after an IO error. The clear commit
     * retries all entrances together; a failed write never destroys the
     * previously verified bank or changes a player's ordinary save slot. */
    memcpy(seeds[i],seed,CHAPTER_SEED_BYTES);present|=1u<<i;return store();
}
bool chapter_progress_clear(void){
    uint8_t previous=cleared;cleared=1;
    if(store())return true;
    cleared=previous;return false;
}
bool chapter_progress_read(unsigned chapter,uint8_t seed[CHAPTER_SEED_BYTES]){
    if(chapter<2 || chapter>3 || !seed || !chapter_progress_available(chapter))return false;
    memcpy(seed,seeds[chapter-2],CHAPTER_SEED_BYTES);return true;
}
