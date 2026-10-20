#include "native_opening.h"
#include "resource_opening.h"
#include "resource_opening_presentation.h"
#include "resource_archive.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
#include <nds.h>
volatile uint32_t native_opening_profile[32][14];
extern uint32_t resource_background_profile[6];
static unsigned profile_index;
static uint32_t profile_cd_cycles;
static unsigned profile_cd_ticks;
#endif
static resource_opening_state *engine;
static uint8_t *bank,*subcode;
static resource_opening_presentation presentation;
unsigned native_opening_picture_height(void){return engine && engine->callbacks.track==3 && engine->scene_id<=2?192:224;}
static uint32_t bcd(unsigned frame){
    unsigned minutes=frame/4500,seconds=frame/75%60,sector=frame%75;
    return ((minutes/10*16+minutes%10)<<24)|((seconds/10*16+seconds%10)<<16)|((sector/10*16+sector%10)<<8);
}
void native_opening_end(void){
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    if(engine)cpuEndTiming();
#endif
    if(engine)resource_background_bind(NULL,0);
    free(engine);free(bank);free(subcode);engine=NULL;bank=subcode=NULL;
    resource_opening_presentation_reset(&presentation);
}
bool native_opening_begin_track(unsigned track,uint32_t rng){
    native_opening_end();if(track!=3 && track!=4)return false;
    FILE *f=fopen(track==3?"nitro:/native-opening-03-r22.nsop":"nitro:/native-opening-04-r22.nsop","rb");if(!f)return false;
    long bytes=-1;if(!fseek(f,0,SEEK_END))bytes=ftell(f);
    bool good=bytes>=32 && bytes<=512*1024 && !fseek(f,0,SEEK_SET);
    bank=good?malloc(bytes):NULL;good=bank && fread(bank,1,bytes,f)==(size_t)bytes && !ferror(f);
    if(fclose(f))good=false;
    if(good){
        f=fopen("nitro:/native-resource-banks-r22.nrs","rb");size_t subbytes=0;
        subcode=f?resource_archive_load(f,"SUBCODE.BIN",&subbytes):NULL;if(f)fclose(f);
        engine=subcode?malloc(sizeof(*engine)):NULL;
        good=engine && resource_opening_bind(engine,bank,bytes,subcode,subbytes,rng) && engine->callbacks.track==track;
    }
    if(!good)native_opening_end();
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    if(good){profile_index=profile_cd_cycles=profile_cd_ticks=0;cpuStartTiming(0);}
#endif
    return good;
}
bool native_opening_begin(uint32_t rng){return native_opening_begin_track(3,rng);}
bool native_opening_tick_cd(unsigned frame,uint16_t status){
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t begin=cpuGetTiming();
#endif
    bool valid=engine && engine->callbacks.track==4 && resource_opening_tick_cd(engine,bcd(frame),1,status) && resource_opening_presentation_tick(&presentation,engine);
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    profile_cd_cycles+=cpuGetTiming()-begin;profile_cd_ticks++;
#endif
    return valid;
}
const uint16_t *native_opening_palette(void){return presentation.colors;}
static bool draw_palette_picture(uint8_t out[256*192]){
    return engine && out && resource_opening_draw_presented8(engine,&presentation,out);
}
bool native_opening_draw_current(uint8_t out[256*192]){
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    if(!engine)return false;
    uint32_t begin=cpuGetTiming();
    for(unsigned i=0;i<6;i++)resource_background_profile[i]=0;
    bool valid=draw_palette_picture(out);uint32_t end=cpuGetTiming();
    volatile uint32_t *p=native_opening_profile[profile_index++&31];
    p[0]=engine->frames-1;p[1]=profile_cd_ticks;p[2]=profile_cd_cycles;p[3]=end-begin;p[4]=engine->scene_id;
    p[5]=engine->scene.transfers;p[6]=engine->front_sprites;p[7]=end;
    for(unsigned i=0;i<6;i++)p[8+i]=resource_background_profile[i];
    profile_cd_cycles=profile_cd_ticks=0;
    return valid;
#else
    return draw_palette_picture(out);
#endif
}
unsigned native_opening_take_cd_request(void){
    if(!engine)return 0;
    unsigned command=engine->cd_request;engine->cd_request=0;return command;
}
bool native_opening_finished(void){return engine && engine->finished;}
bool native_opening_draw(unsigned frame,uint8_t out[256*192]){
    if(!engine || !out)return false;
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t begin=cpuGetTiming();unsigned old=engine->frames;
#endif
    while(engine->frames<=frame){
        unsigned clock=(uint64_t)engine->frames*896040*75/53693175;
        if(!resource_opening_tick(engine,bcd(clock),1))return false;
        if(!resource_opening_presentation_tick(&presentation,engine))return false;
    }
#if defined(__NDS__) && defined(SNATCHER_OPENING_PROFILE)
    uint32_t tick_end=cpuGetTiming();
    for(unsigned i=0;i<6;i++)resource_background_profile[i]=0;
    bool valid=draw_palette_picture(out);uint32_t end=cpuGetTiming();
    volatile uint32_t *p=native_opening_profile[profile_index++&31];
    p[0]=frame;p[1]=engine->frames-old;p[2]=tick_end-begin;p[3]=end-tick_end;p[4]=engine->scene_id;
    p[5]=engine->scene.transfers;p[6]=engine->front_sprites;p[7]=end;
    for(unsigned i=0;i<6;i++)p[8+i]=resource_background_profile[i];
    return valid;
#else
    return draw_palette_picture(out);
#endif
}
