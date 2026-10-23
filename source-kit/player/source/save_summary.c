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
    if(module!=0x5f && module!=0x5e && module!=0x5d && module!=0x5a && module!=0x59 && module!=0x58 && module!=0x57 && module!=0x56 && module!=0x54 && module!=0x53 && module!=0x55 && module!=0x52 && module!=0x51 && module!=0x5c && module!=0x50 && module!=0x4f && module!=0x5b && module!=0x4e && module!=0x4d && module!=0x49 && module!=0x48 && module!=0x47 && module!=0x3c && module!=0x3f && module!=0x40 && module!=0x41 && module!=0x4a && module!=0x42 && module!=0x43 && module!=0x44 && module!=0x45 && module!=0x46)return result;
    result.act=(module==0x5d || module==0x5e || module==0x5f)?3:(module==0x4d || module==0x4e || module==0x4f || module==0x50 || module==0x5b || module==0x5c || module==0x51 || module==0x52 || module==0x55 || module==0x53 || module==0x54 || module==0x56 || module==0x57 || module==0x58 || module==0x59 || module==0x5a)?2:1;
    unsigned scene=hq_scene_under(s->line);
    /* The shared end-of-ACT save routine uses K1/4 as a temporary backing
     * scene. It is not the player's location. Derive this from the saved
     * script too, so existing clear saves need no migration. */
    if((module==0x49 || module==0x5a) && scene==HQ_SCENE(0x1904)){
        result.place=SAVE_PLACE_NONE;return result;
    }
    if(module==0x4e && scene==HQ_SCENE(0x1111))result.place=SAVE_PLACE_CHIEF;
    if((scene&0xff00)==HQ_SCENE(0x1600))result.place=SAVE_PLACE_OPENING;
    if(scene<=1)result.place=SAVE_PLACE_RECEPTION;
    else if(scene==2)result.place=SAVE_PLACE_HALL;
    else if(scene==3 || scene==HQ_SCENE(0x1003))result.place=SAVE_PLACE_CHIEF;
    else if((scene&0xff00)==HQ_SCENE(0x0b00) || (scene&0xff00)==HQ_SCENE(0x0f00))result.place=SAVE_PLACE_ENGINEERING;
    else if((scene&0xff00)==HQ_SCENE(0x0c00))result.place=SAVE_PLACE_COMPUTER;
    else if((scene&0xff00)==HQ_SCENE(0x0d00))result.place=SAVE_PLACE_DETECTIVE;
    else if((scene&0xff00)==HQ_SCENE(0x0e00))result.place=SAVE_PLACE_RANGE;
    else if(((scene&0xff00)==HQ_SCENE(0x2000) || (scene&0xff00)==HQ_SCENE(0x2100) || (scene&0xff00)==HQ_SCENE(0x2200)))result.place=SAVE_PLACE_VEHICLE;
    else if(scene==HQ_SCENE(0x1905))result.place=SAVE_PLACE_FACTORY_FRONT;
    else if((scene&0xff00)==HQ_SCENE(0x1900))result.place=SAVE_PLACE_FACTORY;
    if((scene&0xff00)==HQ_SCENE(0x0700))result.place=SAVE_PLACE_GIBSON;
    if((scene&0xff00)==HQ_SCENE(0x0000))result.place=SAVE_PLACE_ALTON;
    if((scene&0xff00)==HQ_SCENE(0x1800))result.place=SAVE_PLACE_PLATO;
    if((scene&0xff00)==HQ_SCENE(0x1b00))result.place=SAVE_PLACE_OUTER;
    if((scene&0xff00)==HQ_SCENE(0x1c00))result.place=SAVE_PLACE_APARTMENT;
    if((scene&0xff00)==HQ_SCENE(0x1700))result.place=SAVE_PLACE_IVAN;
    if((scene&0xff00)==HQ_SCENE(0x0500) || (scene&0xff00)==HQ_SCENE(0x0600))result.place=SAVE_PLACE_FREDDY;
    if((scene&0xff00)==HQ_SCENE(0x0100))result.place=SAVE_PLACE_HOSPITAL;
    if((scene&0xff00)==HQ_SCENE(0x1d00) || (scene&0xff00)==HQ_SCENE(0x1e00))result.place=SAVE_PLACE_QUEENS;
    if(module==0x5a){
        if(scene==HQ_SCENE(0x1506))result.place=SAVE_PLACE_HALL;
        if(scene==HQ_SCENE(0x0a02) || scene==HQ_SCENE(0x0a03))result.place=SAVE_PLACE_CHIEF;
    }
    /* ACT3 SP36 uses DATA_Y00 (2500..2509) for the headquarters
     * conversation, instead of the earlier H-series room IDs. Summaries
     * are derived when previewing, including checkpoints from older builds. */
    if(module==0x5d && scene>=HQ_SCENE(0x2500) && scene<=HQ_SCENE(0x2509))
        result.place=SAVE_PLACE_HEADQUARTERS;
    return result;
}
