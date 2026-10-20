#include <stdio.h>
#ifdef __NDS__
#include <filesystem.h>
#endif
#include "hq_scene.h"
static const unsigned codes[]={0x0c01,0x0c02,0x0c05,0x1003,0x0b02,0x0c04,0x0d00,0x0e00,0x0e01,0x0f00,0x0f01,0x0f02,0x0f03,0x0f04,0x2000,0x2003,0x2004,0x1900,0x1903,0x1905,0x1907,0x1908,0x1902,0x1909,0x190a,0x190b,0x190c,0x190d,0x190e,0x800,0x801,0xa00,0xb00,0xc00,0x1000,0x1001,0x1002};
static const unsigned overlays[]={0x0a10,0x0e0f,0x0906,0x0b0d,0x0c0d,0xd,0x20d,0x50d,0x70d,0x900,0x905,0xf0d,0x90d,0x90e};
bool hq_scene_valid(unsigned scene){
    if(scene<=4)return true;
    if(scene>=HQ_SCENE(0x1a00) && scene<=HQ_SCENE(0x1a09))return true;
    if(scene==HQ_OVERLAY(0) || scene==HQ_OVERLAY(0x0f00) || scene==HQ_OVERLAY(0x0700) ||
       scene==HQ_OVERLAY(0x0200) || scene==HQ_OVERLAY(0x0500))return true;
    for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);i++)if(scene==HQ_SCENE(codes[i]))return true;
    for(unsigned i=0;i<sizeof(overlays)/sizeof(overlays[0]);i++)if(scene==HQ_OVERLAY(overlays[i]))return true;
    return false;
}
unsigned hq_scene_under(unsigned scene){return scene==HQ_OVERLAY(0x0b0d) || scene==HQ_OVERLAY(0x0c0d)?HQ_SCENE(0x190a):scene&0x8000?HQ_SCENE(0x0f02):scene==4?3:scene;}
bool hq_scene_read(unsigned scene,uint16_t *pixels){
    if(!hq_scene_valid(scene) || scene<=4 || scene>=0x8000)return false;
#ifdef __NDS__
    if(!nitroFSInit(NULL))return false;
    char name[48];snprintf(name,sizeof(name),"nitro:/hq-%04x.bin",scene);
    FILE *f=fopen(name,"rb");if(!f)return false;
    bool good=fread(pixels,2,256*192,f)==256*192 && fgetc(f)==EOF && !ferror(f);
    if(fclose(f))good=false;
    return good;
#else
    (void)pixels;return false;
#endif
}
