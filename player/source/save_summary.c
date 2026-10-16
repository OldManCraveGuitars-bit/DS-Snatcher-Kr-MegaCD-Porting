#include "save_summary.h"
#include "hq_scene.h"
SaveSummary save_summary(const GameCheckpoint *s){
    SaveSummary result={0,SAVE_PLACE_UNKNOWN};
    if(!s)return result;
    if(s->mode>=8 && s->mode<=10)return (SaveSummary){1,SAVE_PLACE_CHIEF};
    if(s->mode==15)return (SaveSummary){1,SAVE_PLACE_OPENING};
    if(s->mode==16 || s->mode==17)return (SaveSummary){1,SAVE_PLACE_RECEPTION};
    if(s->mode!=18 && s->mode!=19)return result;
    unsigned module=s->story[0]>=3?s->story[22]|((unsigned)s->story[23]<<8):0x3f;
    /* SP06 and SP07 are the currently connected ACT1 scripts. Do not label
     * future unrecognized scripts as ACT1 just because a slot is occupied. */
    if(module!=0x3f && module!=0x40 && module!=0x41)return result;
    result.act=1;
    unsigned scene=hq_scene_under(s->line);
    if(scene<=1)result.place=SAVE_PLACE_RECEPTION;
    else if(scene==2)result.place=SAVE_PLACE_HALL;
    else if(scene==3 || scene==HQ_SCENE(0x1003))result.place=SAVE_PLACE_CHIEF;
    else if((scene&0xff00)==HQ_SCENE(0x0b00) || (scene&0xff00)==HQ_SCENE(0x0f00))result.place=SAVE_PLACE_ENGINEERING;
    else if((scene&0xff00)==HQ_SCENE(0x0c00))result.place=SAVE_PLACE_COMPUTER;
    else if((scene&0xff00)==HQ_SCENE(0x0d00))result.place=SAVE_PLACE_DETECTIVE;
    else if((scene&0xff00)==HQ_SCENE(0x0e00))result.place=SAVE_PLACE_RANGE;
    else if((scene&0xff00)==HQ_SCENE(0x2000))result.place=SAVE_PLACE_VEHICLE;
    else if(scene==HQ_SCENE(0x1905))result.place=SAVE_PLACE_FACTORY_FRONT;
    else if((scene&0xff00)==HQ_SCENE(0x1900))result.place=SAVE_PLACE_FACTORY;
    return result;
}
