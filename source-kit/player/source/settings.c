#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#ifdef __NDS__
#include <fat.h>
#else
#include <direct.h>
#endif
bool settings_buttons=true;
bool settings_scanlines=false;
bool settings_tv_filter=false;
unsigned settings_level=4;
static unsigned sequence,bank;
static char directory[128];
static uint32_t crc(const uint8_t *p,unsigned n){uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}return ~c;}
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static void put(uint8_t *p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
static void path(char *p,unsigned slot){snprintf(p,160,"%s/options-%c.bin",directory,slot?'b':'a');}
void settings_load(void){
    settings_buttons=true;settings_scanlines=settings_tv_filter=false;settings_level=4;sequence=0;bank=1;
#ifdef __NDS__
    const char *drive=fatGetDefaultDrive();if(!drive){directory[0]=0;return;}
    snprintf(directory,sizeof(directory),"%ssnatcher_ds",drive);
#else
    const char *dir=getenv("SNATCHER_TEST_SAVE_DIR");snprintf(directory,sizeof(directory),"%s",dir?dir:"");
#endif
    if(!directory[0])return;
    for(unsigned i=0;i<2;i++){
        char p[160];path(p,i);FILE *f=fopen(p,"rb");if(!f)continue;
        uint8_t b[24];size_t n=fread(b,1,24,f);int tail=fgetc(f);bool bad=ferror(f);fclose(f);
        bool legacy=n==24 && !memcmp(b,"SNDSOPT1",8);
        if(bad || n!=24 || tail!=EOF || (!legacy && memcmp(b,"SNDSOPT2",8)) || b[12]>1 || b[13]>4 || b[14]>(legacy?0:1) || b[15]>(legacy?0:1) || word(b+16)!=crc(b,16) || word(b+20) || !word(b+8))continue;
        if(word(b+8)>sequence){sequence=word(b+8);bank=i;settings_buttons=b[12];settings_level=b[13];settings_scanlines=b[14];settings_tv_filter=b[15];}
    }
}
unsigned settings_volume(void){return settings_level*25;}
bool settings_store(void){
    if(!directory[0] || sequence==UINT32_MAX || settings_level>4)return false;
#ifdef __NDS__
    mkdir(directory,0777);
#else
    _mkdir(directory);
#endif
    uint8_t b[24]={0};memcpy(b,"SNDSOPT2",8);put(b+8,sequence+1);b[12]=settings_buttons;b[13]=settings_level;b[14]=settings_scanlines;b[15]=settings_tv_filter;put(b+16,crc(b,16));char p[160];path(p,!bank);
    FILE *f=fopen(p,"wb");if(!f)return false;bool good=fwrite(b,1,24,f)==24;if(fflush(f))good=false;if(fclose(f))good=false;
    if(good){sequence++;bank=!bank;}return good;
}
