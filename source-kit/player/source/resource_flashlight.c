#include "resource_flashlight.h"
#include "resource_actor.h"
static unsigned word(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static uint32_t longword(const uint8_t *p){return (uint32_t)word(p)<<16|word(p+2);}
static void putword(uint8_t *p,unsigned v){p[0]=v>>8;p[1]=v;}
static void putlong(uint8_t *p,uint32_t v){putword(p,v>>16);putword(p+2,v);}
static bool coordinate(uint32_t v,uint32_t low,uint32_t high){return v>=low || v<high;}
bool resource_flashlight_valid(const uint8_t s[14]){
 return s && coordinate(longword(s),UINT32_C(0xffb90000),UINT32_C(0x450000)) &&
  coordinate(longword(s+4),UINT32_C(0xffd90000),UINT32_C(0x250000));
}
static void move(uint8_t *p,int delta,uint32_t low,uint32_t high){
 uint32_t value=longword(p)+(uint32_t)delta;
 if(coordinate(value,low,high))putlong(p,value);
}
bool resource_flashlight_input(uint8_t s[14],const uint8_t *sub,size_t bytes,
 unsigned held,unsigned history,uint8_t *lock,uint8_t *cursor){
 if(!s || !sub || !lock || !cursor || held>255 || history>255)return false;
 if(history)return true; /* SUB10846 does not disturb a nested command menu. */
 if(held&16){
  s[8]=(uint8_t)held;*lock=255;*cursor=3;
  if(held&1)move(s+4,-512,UINT32_C(0xffd90000),UINT32_C(0x250000));
  if(held&2)move(s+4,512,UINT32_C(0xffd90000),UINT32_C(0x250000));
  if(held&8)move(s,512,UINT32_C(0xffb90000),UINT32_C(0x450000));
  if(held&4)move(s,-512,UINT32_C(0xffb90000),UINT32_C(0x450000));
 }else{
  unsigned x=((word(s)+72)&65535)>>2,y=((word(s+4)+40)&65535)>>2;
  size_t at=0x109e8-0xd400+(size_t)y*36+x;
  if(x>=36 || y>=20 || at>=bytes)return false;
  s[9]=x;s[10]=y;putword(s+12,sub[at]);*lock=0;*cursor=0;
 }
 return true;
}
void resource_flashlight_present(const uint8_t s[14],unsigned mode,uint8_t g[128],resource_scroll_state *scroll){
 if(mode!=1)return;
 unsigned x=word(s),y=(word(s+4)-40)&65535;
 uint32_t rng=longword(g+10);
 if(resource_actor_random(&rng)&32768)x++;
 if(resource_actor_random(&rng)&32768)y++;
 putlong(g+10,rng);g[0x67]=1;resource_scroll_mode(scroll,1);
 putword(scroll->control+0x18,x);putword(scroll->control+0x1c,y);
}
bool resource_flashlight_touch(uint8_t s[14],int x,int y){
 if(!s || x<-71 || x>68 || y<-39 || y>36)return false;
 putlong(s,(uint32_t)x<<16);putlong(s+4,(uint32_t)y<<16);return true;
}
