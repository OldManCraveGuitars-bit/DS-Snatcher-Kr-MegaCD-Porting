/* Portable shared MAIN/YM + SUB PCM + playback FIFO checkpoint, version1. */
#include <stdlib.h>
#define SHARED_HEADER 128u
#define SHARED_PCM_BYTES 174500u
extern unsigned forecourt_fm_export_portable_state(void *,unsigned);
extern bool forecourt_fm_import_state(const void *,unsigned);
static uint32_t shared_get32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t shared_get64(const uint8_t *p){return shared_get32(p)|(uint64_t)shared_get32(p+4)<<32;}
static void shared_put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void shared_put64(uint8_t *p,uint64_t v){shared_put32(p,(uint32_t)v);shared_put32(p+4,(uint32_t)(v>>32));}
static uint32_t shared_crc(const void *data,unsigned bytes){
    const uint8_t *p=data;uint32_t crc=~0u;
    static const uint32_t table[16]={0,0x1db71064,0x3b6e20c8,0x26d930ac,0x76dc4190,0x6b6b51f4,0x4db26158,0x5005713c,
        0xedb88320,0xf00f9344,0xd6d6a3e8,0xcb61b38c,0x9b64c2b0,0x86d3d2d4,0xa00ae278,0xbdbdf21c};
    while(bytes--){crc^=*p++;crc=(crc>>4)^table[crc&15];crc=(crc>>4)^table[crc&15];}
    return ~crc;
}
static bool shared_overlap(const void *a,size_t na,const void *b,size_t nb){
    if(!na || !nb)return false;
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y?y-x<na:x-y<nb;
}
typedef struct {uint8_t *out;const uint8_t *in;unsigned at;} shared_io;
static uint32_t shared_word(shared_io *io,uint32_t v){
    if(io->in)v=shared_get32(io->in+io->at);
    if(io->out)shared_put32(io->out+io->at,v);
    io->at+=4;return v;
}
static uint64_t shared_long(shared_io *io,uint64_t v){
    uint32_t lo=shared_word(io,(uint32_t)v),hi=shared_word(io,(uint32_t)(v>>32));
    return lo|(uint64_t)hi<<32;
}
static void shared_bytes(shared_io *io,uint8_t *p,unsigned n){
    if(io->in)memcpy(p,io->in+io->at,n);
    if(io->out)memcpy(io->out+io->at,p,n);
    io->at+=n;
}
#define SHARED_WORD(x) do{uint32_t v=shared_word(io,(uint32_t)(x));if(io->in){(x)=v;if((uint32_t)(x)!=v)return false;}}while(0)
#define SHARED_LONG(x) do{uint64_t v=shared_long(io,(uint64_t)(x));if(io->in)(x)=v;}while(0)
static bool shared_pcm_fields(shared_io *io,resource_room_audio *pcm){
    resource_pcm_driver *d=&pcm->driver;resource_pcm_chip *c=&d->chip;resource_pcm_output *o=&pcm->output;
    shared_bytes(io,c->ram,sizeof(c->ram));shared_bytes(io,&c->registers[0][0],sizeof(c->registers));
    for(unsigned i=0;i<8;i++)SHARED_WORD(c->position[i]);
    /* Four byte-sized bus selectors retain their canonical byte layout. */
    uint8_t selectors[4]={c->bank,c->channel,c->enabled,c->off};
    shared_bytes(io,selectors,4);
    if(io->in){c->bank=selectors[0];c->channel=selectors[1];c->enabled=selectors[2];c->off=selectors[3];}
    shared_bytes(io,d->ram,sizeof(d->ram));shared_bytes(io,d->storage,sizeof(d->storage));
    SHARED_WORD(d->load_id);SHARED_WORD(d->flags);SHARED_WORD(d->attenuation);SHARED_WORD(d->bank.dynamic_bytes);
    SHARED_LONG(o->factor);SHARED_LONG(o->next);SHARED_LONG(o->clocks);
    for(unsigned i=0;i<2;i++)SHARED_WORD(o->previous[i]);
    for(unsigned i=0;i<32;i++)for(unsigned c=0;c<2;c++)SHARED_WORD(o->delta[i][c]);
    for(unsigned i=0;i<2;i++)SHARED_LONG(o->integrator[i]);
    SHARED_WORD(o->cursor);SHARED_WORD(o->rate);
    return io->at==SHARED_PCM_BYTES;
}
#undef SHARED_WORD
#undef SHARED_LONG
static bool shared_pcm_valid(const resource_room_audio *pcm){
    const resource_pcm_driver *d=&pcm->driver;const resource_pcm_chip *c=&d->chip;const resource_pcm_output *o=&pcm->output;
    if(d->error || c->bank>=16 || c->channel>=8 || c->enabled>1 || (d->flags&~15u) ||
       d->bank.dynamic_bytes>sizeof(d->storage) || (d->load_id!=65535 && (!d->load_id || d->load_id>=d->bank.table[0])) ||
       o->rate!=32000 || o->factor!=UINT64_C(4427218577690293) || o->next>=o->factor || o->cursor>=32 ||
       o->previous[0]<-32768 || o->previous[0]>32767 || o->previous[1]<-32768 || o->previous[1]>32767)return false;
    unsigned mode=d->ram[0x11],kind=d->ram[0x0f];
    if((mode!=0 && mode!=1 && mode!=2 && mode!=0x82) || kind>2)return false;
    if(kind){
        const uint8_t *p=d->ram+0x28;
        uint32_t address=(uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];p+=8;
        uint32_t count=(uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];
        if((address&2047) || address<0x65000 || !count ||
           (kind==1 && (address>=0x75000 || count>32)) ||
           (kind==2 && (address>=0x7f000 || count>(0x7f000-address)/2048)))return false;
    }
    /* These legacy timeline members are unused by the shared live session.
     * Reject a mixed owner instead of silently dropping another device. */
    return !pcm->samples && !pcm->ticks && !pcm->next_tick && !pcm->drum_commands && !pcm->drum_count &&
        !pcm->drum_loop_start && !pcm->drum_sequence_delta && !pcm->drum_samples && !pcm->drum_ticks &&
        !pcm->next_drum_tick && !pcm->drum_loops;
}
static bool shared_clock_valid(const resource_shared_sound *s){
    if(s->error || !s->initialized || s->notice_count>RESOURCE_SHARED_SOUND_NOTICES || s->pcm_count>4 || s->notice_read>=RESOURCE_SHARED_SOUND_NOTICES || s->pcm_read>=4 ||
       s->sequence>15 || s->played>s->samples || s->samples-s->played>RESOURCE_SHARED_SOUND_AHEAD_MAX)return false;
    if(!s->samples){if(s->next_frame || s->generated_frame)return false;}
    else if(!s->next_frame || resource_shared_sound_boundary(s->next_frame-1)>=s->samples ||
            resource_shared_sound_boundary(s->next_frame)<s->samples)return false;
    uint64_t first=s->next_frame?s->next_frame:1;
    if(first+s->pcm_count!=(uint64_t)s->generated_frame+1 || s->notice_count>s->generated_frame)return false;
    unsigned consumed=s->generated_frame-s->notice_count;
    if(resource_shared_sound_boundary(consumed)>s->played ||
       (consumed<s->generated_frame && resource_shared_sound_boundary(consumed+1)<=s->played))return false;
    for(unsigned i=0;i<s->notice_count;i++)
        if(s->notices[(s->notice_read+i)&RESOURCE_SHARED_SOUND_NOTICE_MASK].frame!=consumed+i+1)return false;
    for(unsigned i=0;i<s->pcm_count;i++)
    {
        const resource_sound_notification *n=&s->pending_pcm[(s->pcm_read+i)&3];
        if(n->frame!=first+i)return false;
        /* Exactly at a source boundary, played() may already have consumed
         * its notification while the PCM tick belongs to the next sample. */
        if(n->frame<=consumed){
            if(n->frame!=consumed || resource_shared_sound_boundary(n->frame)!=s->samples || s->played!=s->samples)return false;
            continue;
        }
        const resource_sound_notification *a=&s->notices[(s->notice_read+n->frame-consumed-1)&RESOURCE_SHARED_SOUND_NOTICE_MASK];
        if(a->frame!=n->frame || a->pcm!=n->pcm || a->signal!=n->signal || a->music_level!=n->music_level)return false;
    }
    const resource_pcm_output *o=&s->pcm.output;
    if(o->cursor!=(s->samples&31) || o->clocks<s->samples || o->clocks>s->samples+s->samples/50+2 ||
       o->factor/2+o->clocks*o->factor-s->samples*(UINT64_C(1)<<52)!=o->next)return false;
    return shared_pcm_valid(&s->pcm);
}
static void shared_note_put(uint8_t *p,const resource_sound_notification *n){
    shared_put32(p,n->frame);shared_put32(p+4,n->pcm);p[8]=n->signal;p[9]=n->music_level;p[10]=p[11]=0;
}
static bool shared_note_get(const uint8_t *p,resource_sound_notification *n,unsigned version){
    if((version==1 && p[9]) || p[10] || p[11])return false;
    n->frame=shared_get32(p);n->pcm=shared_get32(p+4);n->signal=p[8];n->music_level=version==1?255:p[9];return true;
}
unsigned resource_shared_sound_export(const resource_shared_sound *s,const int16_t *unplayed,unsigned frames,void *data,unsigned capacity){
    if(!s || shared_owner!=s || !shared_clock_valid(s) || !data || frames!=s->samples-s->played || (frames && !unplayed) ||
       s->pcm.driver.bank.dynamic!=s->pcm.driver.storage ||
       s->pcm.driver.bank.table_bytes>UINT32_MAX || s->pcm.driver.bank.resident_bytes>UINT32_MAX ||
       shared_overlap(data,capacity,s,sizeof(*s)) || shared_overlap(data,capacity,unplayed,(size_t)frames*4))return 0;
    unsigned prefix=SHARED_HEADER+SHARED_PCM_BYTES+(s->notice_count+s->pcm_count)*12+frames*4;
    if(prefix>capacity || capacity-prefix<128)return 0;
    uint8_t *p=data;unsigned fm=forecourt_fm_export_portable_state(p+prefix,capacity-prefix);if(!fm)return 0;
    const resource_pcm_effect_bank *b=&s->pcm.driver.bank;
    memset(p,0,SHARED_HEADER);memcpy(p,"NSAU",4);shared_put32(p+4,2);shared_put32(p+8,prefix+fm);
    shared_put32(p+16,b->table_bytes);shared_put32(p+20,b->resident_bytes);
    shared_put32(p+24,shared_crc(b->table,b->table_bytes));shared_put32(p+28,shared_crc(b->resident,b->resident_bytes));
    shared_put64(p+32,s->samples);shared_put64(p+40,s->played);
    shared_put32(p+48,s->next_frame);shared_put32(p+52,s->generated_frame);
    shared_put32(p+56,s->music_request);shared_put32(p+60,s->effect_request);
    p[64]=s->music;p[65]=s->effect;p[66]=s->sequence;
    shared_put32(p+68,s->notice_count);shared_put32(p+72,s->pcm_count);shared_put32(p+76,frames);
    shared_put32(p+80,fm);shared_put32(p+84,SHARED_PCM_BYTES);
    shared_io io={p+SHARED_HEADER,NULL,0};
    if(!shared_pcm_fields(&io,(resource_room_audio *)&s->pcm))return 0;
    unsigned at=SHARED_HEADER+SHARED_PCM_BYTES;
    for(unsigned i=0;i<s->notice_count;i++,at+=12)shared_note_put(p+at,&s->notices[(s->notice_read+i)&RESOURCE_SHARED_SOUND_NOTICE_MASK]);
    for(unsigned i=0;i<s->pcm_count;i++,at+=12)shared_note_put(p+at,&s->pending_pcm[(s->pcm_read+i)&3]);
    for(unsigned i=0;i<frames*2;i++){unsigned v=(uint16_t)unplayed[i];p[at++]=v;p[at++]=v>>8;}
    shared_put32(p+12,shared_crc(p+16,prefix+fm-16));return prefix+fm;
}
bool resource_shared_sound_import(resource_shared_sound *s,const void *data,unsigned bytes,int16_t *unplayed,unsigned capacity,unsigned *frames){
    if(!s || shared_owner!=s || !s->initialized || !data || !frames || bytes<SHARED_HEADER+SHARED_PCM_BYTES+128 ||
       shared_overlap(data,bytes,s,sizeof(*s)) || shared_overlap(unplayed,(size_t)capacity*4,s,sizeof(*s)) ||
       shared_overlap(data,bytes,unplayed,(size_t)capacity*4) || shared_overlap(frames,sizeof(*frames),s,sizeof(*s)) ||
       shared_overlap(frames,sizeof(*frames),data,bytes) || shared_overlap(frames,sizeof(*frames),unplayed,(size_t)capacity*4))return false;
    const uint8_t *p=data;const resource_pcm_effect_bank *b=&s->pcm.driver.bank;
    if(memcmp(p,"NSAU",4) || (shared_get32(p+4)!=1 && shared_get32(p+4)!=2) || shared_get32(p+8)!=bytes ||
       shared_get32(p+12)!=shared_crc(p+16,bytes-16) || shared_get32(p+84)!=SHARED_PCM_BYTES ||
       shared_get32(p+16)!=b->table_bytes || shared_get32(p+20)!=b->resident_bytes ||
       shared_get32(p+24)!=shared_crc(b->table,b->table_bytes) || shared_get32(p+28)!=shared_crc(b->resident,b->resident_bytes) || p[67])return false;
    for(unsigned i=88;i<SHARED_HEADER;i++)if(p[i])return false;
    unsigned notices=shared_get32(p+68),pending=shared_get32(p+72),ahead=shared_get32(p+76),fm=shared_get32(p+80);
    if(notices>RESOURCE_SHARED_SOUND_NOTICES || pending>4 || ahead>RESOURCE_SHARED_SOUND_AHEAD_MAX || ahead>capacity || (ahead && !unplayed))return false;
    unsigned prefix=SHARED_HEADER+SHARED_PCM_BYTES+(notices+pending)*12+ahead*4;
    if(prefix>bytes || fm!=bytes-prefix || fm<128)return false;
    resource_shared_sound *next=calloc(1,sizeof(*next));if(!next)return false;
    next->pcm.driver.bank=*b;next->pcm.driver.bank.dynamic=next->pcm.driver.storage;
    next->samples=shared_get64(p+32);next->played=shared_get64(p+40);
    next->next_frame=shared_get32(p+48);next->generated_frame=shared_get32(p+52);
    next->music_request=shared_get32(p+56);next->effect_request=shared_get32(p+60);
    next->music=p[64];next->effect=p[65];next->sequence=p[66];next->initialized=true;
    next->notice_count=notices;next->pcm_count=pending;
    shared_io io={NULL,p+SHARED_HEADER,0};bool good=shared_pcm_fields(&io,&next->pcm);
    unsigned at=SHARED_HEADER+SHARED_PCM_BYTES;
    for(unsigned i=0;i<notices;i++,at+=12)good=shared_note_get(p+at,next->notices+i,shared_get32(p+4)) && good;
    for(unsigned i=0;i<pending;i++,at+=12)good=shared_note_get(p+at,next->pending_pcm+i,shared_get32(p+4)) && good;
    const uint8_t *f=p+prefix;
    uint64_t divisor=1008u*32000,rem=(next->samples%divisor)*53693175;
    uint64_t expected_clock=((next->samples/divisor)*53693175+rem/divisor)*1008;
    good=good && shared_clock_valid(next) && ahead==next->samples-next->played &&
        !memcmp(f,"NSFL",4) && (shared_get32(f+4)==2 || shared_get32(f+4)==3) && !shared_get32(f+116) &&
        shared_get32(f+60)==(uint32_t)next->samples && shared_get32(f+68)==next->generated_frame &&
        shared_get64(f+48)==expected_clock && shared_get32(f+56)==rem%divisor;
    /* All caller capacities, PCM/queue fields and clock relations are checked
     * before the nested transactional FM import changes its singleton. */
    if(good)good=forecourt_fm_import_state(f,fm);
    if(good){
        *s=*next;s->pcm.driver.bank.dynamic=s->pcm.driver.storage;
        for(unsigned i=0;i<ahead*2;i++){unplayed[i]=(int16_t)(p[at]|(unsigned)p[at+1]<<8);at+=2;}
        *frames=ahead;forecourt_fm_observe(observe,s);
    }
    free(next);return good;
}
