#include "native_startup.h"
#include "resource_sega.h"
#include "resource_konami.h"
#include "resource_background.h"
#include "title_badge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *banks[3];
static size_t sizes[3];
static unsigned phase,konami_frame;
static bool indexed_request;
static uint16_t *copyright_pixels;
static bool copyright_ready,konami_ready;
static uint8_t *read_bank(const char *path,size_t *bytes){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}
    long n=ftell(f);if(n<44 || n>1024*1024 || fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    uint8_t *p=malloc(n);
    if(!p || fread(p,1,n,f)!=(size_t)n || ferror(f)){free(p);fclose(f);return NULL;}
    fclose(f);*bytes=n;return p;
}
void native_startup_end(void){
    for(unsigned i=0;i<3;i++){free(banks[i]);banks[i]=NULL;sizes[i]=0;}
    phase=99;konami_frame=0;
    copyright_ready=konami_ready=false;
    free(copyright_pixels);copyright_pixels=NULL;
}
bool native_startup_begin(void){
    native_startup_end();
    const char *names[3]={"nitro:/native-sega-license-r22.nsbg","nitro:/native-copyright-r22.nsbg","nitro:/native-konami-startup-r22.nsbg"};
    for(unsigned i=0;i<3;i++)if(!(banks[i]=read_bank(names[i],sizes+i))){native_startup_end();return false;}
    if(!resource_sega_bind(banks[0],sizes[0])){native_startup_end();return false;}
    phase=0;return true;
}
bool native_startup_title(uint16_t output[256*192]){
    if(!output)return false;
    size_t bytes;uint8_t *p=read_bank("nitro:/native-title-r22.nsbg",&bytes);
    bool valid=p && resource_background_bind(p,bytes) && resource_background_draw(output);
    resource_background_bind(NULL,0);free(p);
    if(valid)title_badge_draw(output);
    return valid;
}
static bool draw_startup(unsigned frame,void *output){
    if(!banks[0] || !output || frame>=NATIVE_STARTUP_FRAMES)return false;
    unsigned target=frame<NATIVE_STARTUP_COPYRIGHT_FIRST?0:frame<NATIVE_STARTUP_COPYRIGHT_END?1:frame<NATIVE_STARTUP_KONAMI_FIRST?2:3;
    if(target!=phase){
        if(target!=1){free(copyright_pixels);copyright_pixels=NULL;copyright_ready=false;}
        if(target==1 && (!resource_background_bind(banks[1],sizes[1]) ||
                       !resource_background_view(0,16,256,192)))return false;
        if(target==3 && !konami_ready){
            if(!resource_konami_bind(banks[2],sizes[2]))return false;
            konami_frame=0;konami_ready=true;
        }
        phase=target;
    }
    if(target==0)return resource_sega_draw(frame,output);
    if(target==1){
        if(!copyright_ready){
            if(!copyright_pixels)copyright_pixels=malloc(256*192*2);
            if(!copyright_pixels)return false;
            if(!resource_background_draw(copyright_pixels))return false;
            copyright_ready=true;
        }
        /* Prepare original tile caches during the stationary copyright hold.
         * The first animated KONAMI VBlank then needs no cold full-plane load. */
        if(frame>=NATIVE_STARTUP_KONAMI_FIRST-30 && !konami_ready){
            if(!resource_konami_bind(banks[2],sizes[2]) || !resource_konami_draw(output))return false;
            konami_frame=0;konami_ready=true;
        }
        memcpy(output,copyright_pixels,256*192*2);return true;
    }
    if(target==2){
        uint16_t backdrop=resource_background_has_ui_backdrop()?0:0x8000;
        for(unsigned i=0;i<256*192;i++)((uint16_t*)output)[i]=backdrop;
        return true;
    }
    unsigned tick=frame-NATIVE_STARTUP_KONAMI_FIRST+1;
    while(konami_frame<tick){
        ++konami_frame;
        if(resource_konami_tick(konami_frame&255)<0)return false;
        (void)resource_konami_music_command(); /* The same177 request clocks NST1. */
    }
    return indexed_request?resource_konami_draw_indexed((uint8_t*)output):resource_konami_draw(output);
}
bool native_startup_draw(unsigned frame,uint16_t output[256*192]){
    return draw_startup(frame,output);
}
bool native_startup_draw_indexed(unsigned frame,uint8_t output[256*192]){
    if(frame<NATIVE_STARTUP_KONAMI_FIRST || !output)return false;
    indexed_request=true;
    bool valid=draw_startup(frame,output);
    indexed_request=false;return valid;
}
const uint16_t *native_startup_palette(void){return resource_background_palette_colors();}
