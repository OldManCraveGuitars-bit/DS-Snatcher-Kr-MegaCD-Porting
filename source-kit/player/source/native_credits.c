#include "native_credits.h"
#include "resource_credits_scene.h"
#include "resource_cdda_stream.h"
#include "resource_archive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static resource_credits_scene *engine;
static resource_background_picture picture;
static resource_cdda_stream cd;
static uint8_t *bank,*sub;
static unsigned written,track,ticks;
static uint8_t special_mode,pending_input;
static bool failed;
static unsigned picture_scene,requested_scene,scene_clear,quote_lines;
static bool scene_pending;
static int16_t scratch[1024*2];
static bool request(void){
    unsigned command=engine->video.cd_request;
    if(!command)return true;
    engine->video.cd_request=0;
    if((command!=19 && command!=2) || (track && cd.produced!=cd.frames))return false;
    char path[80];snprintf(path,sizeof(path),"nitro:/native-cdda-%02u-r22.nsta",command);
    resource_cdda_close(&cd);
    if(!resource_cdda_open(&cd,path,written))return false;
    track=command;return true;
}
void native_credits_end(void){
    resource_cdda_close(&cd);
    if(engine)resource_background_bind(NULL,0);
    free(engine);free(bank);free(sub);engine=NULL;bank=sub=NULL;
    memset(&picture,0,sizeof(picture));written=track=ticks=0;failed=false;pending_input=0;
    picture_scene=requested_scene=scene_clear=quote_lines=0;scene_pending=false;
}
bool native_credits_begin(uint32_t rng,uint8_t special){
    native_credits_end();special_mode=special;
    FILE *f=fopen("nitro:/native-credits-r22.bin","rb");
    size_t n=182794,sn=0;bank=f?malloc(n):NULL;
    bool ok=bank && fread(bank,1,n,f)==n && fgetc(f)==EOF && !ferror(f);
    if(f && fclose(f))ok=false;
    f=ok?fopen("nitro:/native-resource-banks-r22.nrs","rb"):NULL;
    sub=f?resource_archive_load(f,"SUBCODE.BIN",&sn):NULL;if(f)fclose(f);
    engine=sub?malloc(sizeof(*engine)):NULL;
    ok=engine && resource_credits_scene_begin(engine,bank,n,sub,sn,rng) &&
        resource_credits_scene_tick(engine,0,0,special_mode,0) && request();
    if(!ok){native_credits_end();return false;}
    ticks=1;return true;
}
bool native_credits_fill(int16_t *out,unsigned frames){
    if(!engine || failed || !out)return false;
    memset(out,0,frames*4);
    if(!resource_cdda_mix(&cd,out,frames,scratch,1024)){failed=true;return false;}
    written+=frames;return true;
}
bool native_credits_waiting(void){return engine && engine->control.index==25 && !engine->video.finished;}
unsigned native_credits_quote_lines(void){return quote_lines;}
void native_credits_input(bool pressed){if(pressed && native_credits_waiting())pending_input=128;}
unsigned native_credits_track(void){return track;}
unsigned native_credits_position(unsigned played){unsigned p=0;resource_cdda_position(&cd,played,&p);return p;}
const uint16_t *native_credits_palette(void){return engine?engine->presentation.colors:NULL;}
int native_credits_draw(unsigned played,uint8_t out[256*192]){
    if(!engine || failed || cd.failed || !out)return -1;
    unsigned target=(uint64_t)played*53693175/(UINT64_C(32000)*896040);
    while(ticks<=target){
        unsigned sample=(uint64_t)ticks*32000*896040/53693175;
        unsigned pos=0,status=0;
        if(resource_cdda_position(&cd,sample,&pos)){
            status=pos>=cd.frames?0x500:
                (engine->control.index<0 || engine->control.index==21)?0x100:0x200;
        }
        if(!resource_credits_scene_tick(engine,resource_cdda_clock(&cd,sample),status,special_mode,pending_input) || !request())return -1;
        /* Scene requests precede their VDP publication. Change the crop
         * only when the old plane has actually been cleared. */
        if(scene_pending && engine->video.published_clears!=scene_clear){picture_scene=requested_scene;scene_pending=false;}
        if(engine->video.scene_id!=requested_scene){
            requested_scene=engine->video.scene_id;scene_clear=engine->video.published_clears;scene_pending=true;
        }
        pending_input=0;ticks++;
        if(engine->video.finished)return 1;
    }
    bool changed=false;
    quote_lines=0;
    if(picture_scene==7){
        /* All five source lines fit y16..207. Remove only sixteen blank
         * rows at either edge; never discard a thin serif scanline. */
        if(!resource_opening_draw_cinematic_indices(&engine->video,out,
            engine->presentation.colors,&picture,&changed,0,256,192,16))return -1;
        static const unsigned rows[5]={37,69,101,133,166};
        for(unsigned line=0;line<5;line++)for(unsigned x=24;x<232;x++){
            unsigned at=(rows[line]-16)*256+x;
            if(engine->presentation.colors[out[at]&63]&32767){quote_lines|=1u<<line;break;}
        }
    }else if(!resource_opening_draw_full_indices_cached(&engine->video,out,
        engine->presentation.colors,&picture,&changed))return -1;
    return 0;
}
