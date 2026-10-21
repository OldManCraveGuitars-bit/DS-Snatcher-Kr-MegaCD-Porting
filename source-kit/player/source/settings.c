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
bool settings_bgm=true,settings_effects=true,settings_voice=true;
static unsigned sequence,bank;
static char directory[128];
static uint32_t crc(const uint8_t *p,unsigned n){uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}return ~c;}
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static void put(uint8_t *p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
static void path(char *p,unsigned slot){snprintf(p,160,"%s/options-%c.bin",directory,slot?'b':'a');}
void settings_load(void){
    settings_buttons=true;settings_scanlines=settings_tv_filter=false;settings_level=4;settings_bgm=settings_effects=settings_voice=true;sequence=0;bank=1;
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
        bool audio=n==24 && !memcmp(b,"SNDSOPT3",8);
        if(bad || n!=24 || tail!=EOF || (!legacy && !audio && memcmp(b,"SNDSOPT2",8)) || b[12]>(audio?15:1) || b[13]>4 || b[14]>(legacy?0:1) || b[15]>(legacy?0:1) || word(b+16)!=crc(b,16) || word(b+20) || !word(b+8))continue;
        if(word(b+8)>sequence){sequence=word(b+8);bank=i;settings_buttons=b[12]&1;settings_level=b[13];settings_scanlines=b[14];settings_tv_filter=b[15];
            settings_bgm=!audio || (b[12]&2);settings_effects=!audio || (b[12]&4);settings_voice=!audio || (b[12]&8);}
    }
}
unsigned settings_volume(void){return settings_level*25;}
unsigned settings_voice_volume(void){return settings_voice?settings_volume():0;}
unsigned settings_pcm_mask(bool speech){
    /* Original SUB: pairs0/1 are streamed speech OR effects, pairs2/3 drums. */
    return ((speech?settings_voice:settings_effects)?15u:0u)|(settings_bgm?240u:0u);
}
unsigned settings_fm_mask(unsigned original_effect_flags){
    /* Original MAIN FF4E08/FF4EB6: only FM4/5 are lent to the two SFX slots.
     * Read their ownership, never suppress requests or change original RAM. */
    unsigned effects=(original_effect_flags&3u)<<4;
    return (settings_bgm?(63u&~effects):0u)|(settings_effects?effects:0u);
}
bool settings_store(void){
    if(!directory[0] || sequence==UINT32_MAX || settings_level>4)return false;
#ifdef __NDS__
    mkdir(directory,0777);
#else
    _mkdir(directory);
#endif
    uint8_t b[24]={0};memcpy(b,"SNDSOPT3",8);put(b+8,sequence+1);b[12]=settings_buttons|(settings_bgm<<1)|(settings_effects<<2)|(settings_voice<<3);b[13]=settings_level;b[14]=settings_scanlines;b[15]=settings_tv_filter;put(b+16,crc(b,16));char p[160];path(p,!bank);
    FILE *f=fopen(p,"wb");if(!f)return false;bool good=fwrite(b,1,24,f)==24;if(fflush(f))good=false;if(fclose(f))good=false;
    if(good){sequence++;bank=!bank;}return good;
}
