#include "resource_callback.h"
#include <string.h>
static uint16_t le16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t le32(const uint8_t *p){return le16(p)|(uint32_t)le16(p+2)<<16;}
static bool range(size_t bytes,uint32_t at,size_t n){return at<=bytes && n<=bytes-at;}
static bool validate_event(const resource_callback_state *s,const uint8_t *p,size_t bytes,const uint8_t *event,unsigned version){
    unsigned n=le32(event+4),at=le32(event+8);
    if(n>256 || !range(bytes,at,n*12))return false;
    for(unsigned k=0;k<n;k++){
        const uint8_t *j=p+at+k*12;unsigned kind=le16(j),size=le16(j+2);uint32_t address=le32(j+4),value=le32(j+8);
        if(kind<1 || kind>(version==1?5:8))return false;
        if(kind<5 && (size || address))return false;
        if(kind==2 && value>=32)return false;
        if(kind==3 && (value<s->base || value-s->base>s->resource_size || s->resource_size-(value-s->base)<4))return false;
        if(kind==5 && ((size!=1 && size!=2 && size!=4) || !((address>=0x7800 && address<=0x10000-size) || (version>=2 && address>=0xb6000 && address<=0xb6500-size))))return false;
        if(kind==6 && (size!=1 || address!=0xff800f || value!=16))return false;
        if(kind==7 && (size || address || !(value==4 || (version==3 && value==5))))return false;
        if(kind==8 && (k || size || address!=0x100 || (value&65535)+(value>>16)!=n-1))return false;
    }
    return true;
}
bool resource_callback_bind(resource_callback_state *s,const void *data,size_t bytes){
    if(!s)return false;
    memset(s,0,sizeof(*s));
    const uint8_t *p=data;
    if(!p || bytes<32 || memcmp(p,"NSOP",4))return false;
    unsigned version=le16(p+4);if(version<1 || version>3 || (version==3 && bytes<40))return false;
    s->track=le16(p+6);s->file_id=le32(p+8);unsigned events=le32(p+12);
    s->directory=le32(p+16);s->resource_offset=le32(p+20);s->resource_size=le32(p+24);s->base=le32(p+28);
    if(!((version==1 && s->track==3 && s->file_id==3) || (version>=2 && s->track==4 && s->file_id==2)) || !events || events>128 || s->base!=0x28000 ||
       !range(bytes,s->directory,events*16) || !range(bytes,s->resource_offset,s->resource_size))return false;
    for(unsigned i=0;i<events;i++)if(!validate_event(s,p,bytes,p+s->directory+i*16,version))return false;
    if(version==3){
        unsigned at=le32(p+32),n=le32(p+36);
        if(n!=2 || !range(bytes,at,n*16))return false;
        for(unsigned i=0;i<n;i++)if(!validate_event(s,p,bytes,p+at+i*16,version))return false;
        s->index=-1;
    }
    s->bank=p;s->bytes=bytes;s->events=events;return true;
}
static int run(resource_callback_state *s,const uint8_t *event,uint16_t cd_status,resource_callback_job job,void *context){
    unsigned count=le32(event+4),at=le32(event+8);s->executed++;
    if(count && le16(s->bank+at)==8){
        uint32_t branches=le32(s->bank+at+8),ready=branches&65535,waiting=branches>>16;
        at+=12;if(cd_status==0x100)count=ready;else{at+=ready*12;count=waiting;}
    }
    for(unsigned i=0;i<count;i++){
        const uint8_t *j=s->bank+at+i*12;unsigned kind=le16(j),size=le16(j+2);uint32_t address=le32(j+4),value=le32(j+8);
        if(!job(context,kind,size,address,value))return -1;
        if(kind==5 && address==0x7896 && size==2)s->index=(int16_t)value;
    }
    return 1;
}
int resource_callback_tick_cd(resource_callback_state *s,uint32_t clock,uint16_t cd_status,resource_callback_job job,void *context){
    if(!s || !s->bank || !job || s->index<0 || (unsigned)s->index>=s->events)return -1;
    const uint8_t *event=s->bank+s->directory+s->index*16;
    if(le32(event)>clock)return 0;
    s->index++;return run(s,event,cd_status,job,context);
}
int resource_callback_initialize(resource_callback_state *s,uint16_t phase,uint16_t status,resource_callback_job job,void *ctx){
    if(!s || !s->bank || !job || s->index!=-1 || phase>1 || le16(s->bank+4)!=3)return -1;
    return run(s,s->bank+le32(s->bank+32)+phase*16,status,job,ctx);
}
int resource_callback_tick(resource_callback_state *s,uint32_t clock,resource_callback_job job,void *context){
    return resource_callback_tick_cd(s,clock,0,job,context);
}
