#include "resource_pcm_driver.h"
#include <string.h>
static unsigned word(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static uint32_t longword(const uint8_t *p){return (uint32_t)word(p)<<16|word(p+2);}
static void putword(uint8_t *p,unsigned v){p[0]=v>>8;p[1]=v;}
static void putlong(uint8_t *p,uint32_t v){putword(p,v>>16);putword(p+2,v);}
static uint8_t *slot(resource_pcm_driver *s,unsigned pair){return s->ram+0x100+pair*64;}
static void load_request(resource_pcm_driver *s,uint32_t address,uint32_t sector,uint32_t count,unsigned kind);
static void select_channel(resource_pcm_driver *s,unsigned channel){resource_pcm_chip_write(&s->chip,7,0xc0|channel);}
static void channels(resource_pcm_driver *s){
    if(s->ram[2]!=s->ram[3]){s->ram[3]=s->ram[2];resource_pcm_chip_write(&s->chip,8,s->ram[2]);}
}
static void stop(resource_pcm_driver *s,unsigned pair){
    uint8_t *p=slot(s,pair);p[0x14]=0;p[0]&=~1;
    s->ram[2]|=3u<<(pair*2);channels(s);
}
static void loop_address(resource_pcm_driver *s,unsigned pair,unsigned address){
    unsigned count=pair==0 && (s->ram[0x12]&3)==2?4:2;
    for(unsigned c=pair*2;c<pair*2+count;c++){
        select_channel(s,c);resource_pcm_chip_write(&s->chip,5,address>>8);resource_pcm_chip_write(&s->chip,4,address);
    }
}
static bool source_byte(resource_pcm_driver *s,uint32_t address,uint8_t *value){
    const uint8_t *p;size_t size,offset;
    if(address>=0x55000 && address<0x65000){p=s->bank.resident;size=s->bank.resident_bytes;offset=address-0x55000;}
    else if(address>=0x65000 && address<0x7f800){p=s->bank.dynamic;size=s->bank.dynamic_bytes;offset=address-0x65000;}
    else return false;
    if(!p || offset>=size)return false;
    *value=p[offset];return true;
}
/* SUB15BE2: refill two4KiB banks, retaining native wrap sentinels,
 * loop-pointer updates and the terminal byte's exact address. */
static bool upload(resource_pcm_driver *s,unsigned pair,unsigned destination){
    uint8_t *p=slot(s,pair);uint32_t source=longword(p+2);
    bool speech=s->ram[0x11]==2 || s->ram[0x11]==0x82;
    putword(p+10,destination+8192);bool last_bank=false;
    for(unsigned block=0;block<2;block++){
        resource_pcm_chip_write(&s->chip,7,0x80+(destination>>12));
        last_bank=speech?destination==0x7000:(destination&0x3000)==0x3000;
        unsigned size=last_bank?4095:4096;
        if(last_bank)resource_pcm_chip_write(&s->chip,0x1fff,255);
        for(unsigned i=0;i<size;i++){
            uint8_t value;if(!source_byte(s,source++,&value))return false;
            resource_pcm_chip_write(&s->chip,0x1000+i,value);
            if(value==255){
                /* Native A3 low13bits wrap after the last word. */
                unsigned end=(destination+((i+1)&4095)-2)&65535;
                putword(p+0x12,end);
                if((end&(speech?32767:16383))<8192)p[0]|=2;else loop_address(s,pair,end);
                p[0]|=1;return true;
            }
        }
        destination=(destination+4096)&65535;
    }
    if(last_bank){destination=(destination-(speech?32768:16384))&65535;source++;loop_address(s,pair,destination);}
    if(source==0x75000)source=0x65000;
    putlong(p+2,source);putword(p+10,destination);putword(p+0x12,destination);s->ram[0x47]++;
    return true;
}
static void configure(resource_pcm_driver *s,unsigned pair,unsigned channel){
    uint8_t *p=slot(s,pair);select_channel(s,channel);
    unsigned volume=p[0x17];if(s->attenuation)volume=volume*0x70>>8;
    if(pair>=2 && s->ram[0x17]!=255)volume=volume*s->ram[0x17]>>8;
    unsigned pan=((unsigned)p[0x18]<<4)|(p[0x18]>>4);pan&=255;
    if(s->ram[6]){pan=((pan&15)+(pan>>4))/2;pan|=pan<<4;}
    resource_pcm_chip_write(&s->chip,0,volume);resource_pcm_chip_write(&s->chip,1,pan);
    resource_pcm_chip_write(&s->chip,3,p[0x1a]);resource_pcm_chip_write(&s->chip,2,p[0x1b]);
    resource_pcm_chip_write(&s->chip,6,pair*64);
}
static void history_clear(resource_pcm_driver *s){memset(s->ram+0x60,255,26);putword(s->ram+0x7a,0);}
static void history_add(resource_pcm_driver *s,uint32_t source){
    unsigned at=word(s->ram+0x7a);putword(s->ram+0x60+at,source-0x65000);
    at+=2;if(at==26)at=0;putword(s->ram+0x60+at,65535);putword(s->ram+0x7a,at);
}
static bool speech_ready(resource_pcm_driver *s){
    if(s->ram[0x0f] || !(s->flags&1))return true;
    uint8_t *p=slot(s,0);p[0x17]=s->ram[8];p[0x18]=s->ram[9];putword(p+0x1a,0x400);putlong(p+2,0x65000);
    for(unsigned i=0;i<4;i++)configure(s,0,i);
    if(!upload(s,0,0))return false;
    s->ram[0x7c]=1;history_clear(s);history_add(s,longword(p+2));s->ram[2]&=0xf0;
    s->flags=(s->flags|2)&~1;p[0x14]=s->ram[7];return true;
}
static void speech_stop(resource_pcm_driver *s){
    uint8_t *p=slot(s,0);p[0x14]=0;p[0]&=~1;s->ram[2]|=15;channels(s);
}
static void speech_poll(resource_pcm_driver *s){
    uint8_t *p=slot(s,0);if(!p[0x14])return;
    unsigned address=resource_pcm_chip_read(&s->chip,16)|(unsigned)resource_pcm_chip_read(&s->chip,17)<<8;
    if(!s->ram[0x1b]){
        putword(p+0x10,address);
        if((p[0]&2) && (address&32767)<8192){p[0]&=~2;loop_address(s,0,word(p+0x12));}
        unsigned end=word(p+0x12);
        if(end==address){
            if(s->ram[0x11]==2){s->ram[0x13]=(s->ram[0x13]&~1)|2;s->ram[0x14]=0;}
            s->ram[0x11]=0;speech_stop(s);s->flags&=~2;return;
        }
        if(end>address){if(end-address<8192 && !(p[0]&1))p[0]|=4;return;}
        if(address-end<24576 || (p[0]&1))return;
    }
    if(s->ram[0x1b]){
        if(s->ram[0x0f])return;
        s->ram[0x1b]=0;
        for(unsigned i=0;i<4;i++){select_channel(s,i);resource_pcm_chip_write(&s->chip,6,s->ram[0]);}
        loop_address(s,0,0);s->ram[2]|=15;channels(s);s->ram[2]&=0xf0;channels(s);
    }else if(s->ram[0x0f]){
        s->ram[0x1b]=1;address=(address+512)&0xff00;putword(s->ram,address);address=(address-2)&65535;
        resource_pcm_chip_write(&s->chip,7,0x80+(address>>12));
        resource_pcm_chip_write(&s->chip,0x1000+(address&4095),128);
        resource_pcm_chip_write(&s->chip,0x1001+(address&4095),255);loop_address(s,0,address);
    }else p[0]|=4;
}
static unsigned history_next(resource_pcm_driver *s,unsigned *index){
    if(*index>=13)return 65535;
    return word(s->ram+0x60+(*index)++*2);
}
static void speech_reads(resource_pcm_driver *s){
    if(!(s->flags&2) || s->ram[0x7d] || s->ram[0x0f] || !s->ram[0x0e])return;
    unsigned i=0,value=history_next(s,&i),address;
    if(value==65535)return;
    if(s->ram[0x7c]){
        for(unsigned expected=0x2000;expected<=0xa000;expected+=0x2000){
            if(value==expected)value=history_next(s,&i);
            if(value==65535)return;
        }
        if(value!=0xc000 && value!=0xe000){s->ram[0x43]++;return;}
        s->ram[0x7c]=0;address=0x65000;
    }else if(value==0xe000 || value==0){
        if(value==0xe000){value=history_next(s,&i);if(value==65535)return;}
        if(value==0){value=history_next(s,&i);if(value==65535)return;}
        if(value==0x2000){value=history_next(s,&i);if(value==65535)return;}
        if(value!=0x4000 && value!=0x6000){s->ram[0x43]++;return;}
        address=0x6d000;
    }else{
        if(value!=0x6000 && value!=0x8000){s->ram[0x43]++;return;}
        if(value==0x6000){value=history_next(s,&i);if(value==65535)return;}
        if(value==0x8000){value=history_next(s,&i);if(value==65535)return;}
        if(value==0xa000){value=history_next(s,&i);if(value==65535)return;}
        if(value!=0xc000 && value!=0xe000){s->ram[0x43]++;return;}
        address=0x65000;
    }
    history_clear(s);
    uint32_t sector=longword(s->ram+0x20),count=longword(s->ram+0x24);putlong(s->ram+0x20,sector+16);
    if((count&65535)>=17){putlong(s->ram+0x24,(count-16)&65535);count=16;}
    else{s->ram[0x0e]=0;putlong(s->ram+0x24,0);}
    load_request(s,address,sector,count,1);
}
bool resource_pcm_driver_bind(resource_pcm_driver *s,const resource_pcm_effect_bank *b){
    if(!s || !b || !b->table || b->table_bytes<0x4010 || b->dynamic_bytes>106496 || (b->dynamic_bytes && !b->dynamic))return false;
    memset(s,0,sizeof(*s));s->bank=*b;resource_pcm_chip_reset(&s->chip);
    if(b->dynamic_bytes)memcpy(s->storage,b->dynamic,b->dynamic_bytes);
    s->bank.dynamic=s->storage;s->load_id=65535;
    s->ram[2]=s->ram[3]=s->ram[0x17]=s->ram[0x19]=255;s->ram[0x207]=4;
    /* An effect bank is selected by native load mode1. */
    s->ram[0x11]=s->ram[0x12]=1;return true;
}
static void load_request(resource_pcm_driver *s,uint32_t address,uint32_t sector,uint32_t count,unsigned kind){
    putlong(s->ram+0x28,address);putlong(s->ram+0x2c,sector);putlong(s->ram+0x30,count);s->ram[0x0f]=kind;
}
bool resource_pcm_driver_load(resource_pcm_driver *s,unsigned id){
    if(!s || !s->bank.table || !id || id>=0x4000/8 || id*8+8>s->bank.table_bytes)return false;
    const uint8_t *p=s->bank.table+id*8;unsigned mode=p[1],count=word(p+6);
    if(!count || (mode!=1 && mode!=2 && mode!=0x82) || (mode==1 && count>=53))return false;
    s->load_id=id;putword(s->ram+0x4c,id);s->ram[7]=p[0];s->ram[0x11]=s->ram[0x12]=mode;
    s->ram[8]=p[2];s->ram[9]=p[3];uint32_t sector=word(p+4)+0x17;
    if(mode==1){
        s->ram[0x0e]=0;putlong(s->ram+0x24,0);load_request(s,0x65000,sector,count,2);s->flags|=4;
    }else{
        if(mode==2){s->ram[0x13]=(s->ram[0x13]&~2)|1;s->ram[0x14]=0;}
        s->flags&=~8;stop(s,0);stop(s,1);s->load_id=65535;
        putlong(s->ram+0x20,sector+32);
        if(count>=33){putlong(s->ram+0x24,count-32);count=32;}
        else{s->ram[0x0e]=0;putlong(s->ram+0x24,0);}
        load_request(s,0x65000,sector,count,1);s->flags|=1;
    }
    return true;
}
bool resource_pcm_driver_feed_load(resource_pcm_driver *s,const void *data,size_t bytes){
    if(!s || !data || !s->ram[0x0f] || !bytes || (bytes&2047) || bytes>(uint64_t)longword(s->ram+0x30)*2048)return false;
    unsigned kind=s->ram[0x0f];uint32_t address=longword(s->ram+0x28);
    /* Check the whole request before touching the active wave buffer. */
    if(address<0x65000 || address>=0x65000+sizeof(s->storage) || (address&2047) ||
       (kind==2 && bytes>sizeof(s->storage)-(address-0x65000)) ||
       (kind==1 && (address>=0x75000 || bytes>65536)))return false;
    const uint8_t *p=data;
    while(bytes){
        memcpy(s->storage+address-0x65000,p,2048);p+=2048;bytes-=2048;address+=2048;
        if(kind==1 && address==0x75000)address=0x65000;
        putlong(s->ram+0x28,address);putlong(s->ram+0x2c,longword(s->ram+0x2c)+1);
        putlong(s->ram+0x30,longword(s->ram+0x30)-1);s->ram[0x46]++;
    }
    if(longword(s->ram+0x30))return true;
    s->bank.dynamic_bytes=sizeof(s->storage);
    if(longword(s->ram+0x24))s->ram[0x0e]=1;
    if(kind==2)s->flags&=~4;
    s->ram[0x0f]=0;return true;
}
bool resource_pcm_driver_complete_load(resource_pcm_driver *s,const void *data,size_t bytes){
    if(!s || bytes!=(uint64_t)longword(s->ram+0x30)*2048)return false;
    return resource_pcm_driver_feed_load(s,data,bytes);
}
bool resource_pcm_driver_effect(resource_pcm_driver *s,unsigned command,unsigned pair){
    if(!s)return false;
    s->error=0;command&=255;pair&=255;
    if(!command)return true;
    putword(s->ram+0x4a,command);
    if(command>=0xf8){
        if(command==0xfc)s->ram[6]=1;
        else if(command==0xfd)s->ram[6]=0;
        else if(command==255){
            resource_pcm_chip_write(&s->chip,8,255);s->ram[2]=s->ram[3]=255;s->flags&=~8;
        }else if(command!=0xf8 && command!=0xf9 && command!=0xfa && command!=0xfb && command!=0xfe)return false;
        return true;
    }
    if(command>=s->bank.table[0x4000])return true;
    s->flags|=8;
    size_t at=0x4000+command*16;if(at+16>s->bank.table_bytes){s->error=1;return false;}
    const uint8_t *descriptor=s->bank.table+at;
    if(pair==255){s->ram[7]=descriptor[0];pair=descriptor[1];}
    if(pair>=2)return true;
    uint8_t *p=slot(s,pair);p[1]=command;
    if(descriptor[0]<p[0x14])return true;
    resource_pcm_effect effect;if(!resource_pcm_effect_lookup(&s->bank,command,&effect)){s->error=1;return false;}
    stop(s,pair);s->ram[7]=descriptor[0];p[0x15]=(pair==descriptor[1])?pair:descriptor[1];
    p[0x16]=p[0x17]=effect.volume;p[0x18]=effect.pan;putword(p+0x1a,effect.step);putlong(p+2,effect.address+16);
    for(unsigned c=pair*2;c<pair*2+2;c++)configure(s,pair,c);
    if(!upload(s,pair,pair*16384)){s->error=2;return false;}
    s->ram[2]&=~(3u<<(pair*2));p[0x14]=s->ram[7];return true;
}
static bool resident_drum(unsigned id){return id>0 && id<13 && id!=6 && id!=8 && id!=9 && id!=10;}
bool resource_pcm_driver_drum_bank(resource_pcm_driver *s){
    if(!s)return false;
    unsigned destination=0x8000;
    for(unsigned id=1;id<13;id++){
        if(!resident_drum(id))continue;
        resource_pcm_effect e;
        if(!resource_pcm_effect_lookup(&s->bank,id,&e) || e.address<0x55000 || e.address>=0x65000 ||
           e.samples+4>65536-destination)return false;
        putlong(s->ram+0x240+id*8,destination);
        unsigned i=0;
        do{
            resource_pcm_chip_write(&s->chip,7,0x80+(destination>>12));
            unsigned remain=4096-(destination&4095);
            while(i<e.samples && remain--){
                resource_pcm_chip_write(&s->chip,0x1000+(destination&4095),e.encoded[i++]);destination++;
            }
        }while(i<e.samples);
        /* Native161AC also selects the next bank when the terminator is
         * exactly on a4KiB boundary. The three sentinels share that bank. */
        if(!(destination&4095))resource_pcm_chip_write(&s->chip,7,0x80+(destination>>12));
        if((destination&4095)>4092)return false;
        putlong(s->ram+0x244+id*8,destination);
        resource_pcm_chip_write(&s->chip,0x1000+(destination&4095),128);
        for(unsigned n=1;n<=3;n++)resource_pcm_chip_write(&s->chip,0x1000+(destination&4095)+n,255);
        destination=(destination+4+255)&~255u;
    }
    return true;
}
static bool drum(resource_pcm_driver *s,unsigned id,unsigned pair,unsigned level){
    if(!resident_drum(id))return true;
    size_t at=0x4000+id*16;if(at+16>s->bank.table_bytes)return false;
    const uint8_t *descriptor=s->bank.table+at;uint8_t *p=slot(s,pair);
    p[1]=id;if(descriptor[0]<p[0x14])return true;
    unsigned begin=longword(s->ram+0x240+id*8),end=longword(s->ram+0x244+id*8);
    if(begin<0x8000 || begin>=end || end>=65536)return false;
    stop(s,pair);s->ram[7]=descriptor[0];p[0x15]=descriptor[1];p[0x16]=descriptor[2];
    p[0x17]=level?descriptor[2]*level>>8:descriptor[2];
    p[0x18]=descriptor[3];putword(p+0x1a,word(descriptor+4));
    for(unsigned c=pair*2;c<pair*2+2;c++){
        configure(s,pair,c);resource_pcm_chip_write(&s->chip,6,begin>>8);
    }
    loop_address(s,pair,end);s->ram[2]&=~(3u<<(pair*2));p[0x14]=s->ram[7];return true;
}
bool resource_pcm_driver_drums(resource_pcm_driver *s,uint8_t first,uint8_t second,uint8_t levels,uint8_t sequence){
    if(!s)return false;
    if(s->ram[0x0a]==sequence)return true;
    s->ram[0x0a]=sequence;
    return drum(s,first&31,2,(((second&7)|((levels&0x60)>>2))<<3)&255) &&
           drum(s,second>>3,3,(levels&31)<<3);
}
void resource_pcm_driver_music_level(resource_pcm_driver *s,unsigned level){
    if(!s || level>255 || s->ram[0x17]==level)return;
    s->ram[0x17]=level;
    /*15466 updates envelope only; do not reset sample cursor/start/loop. */
    for(unsigned pair=2;pair<4;pair++){
        uint8_t *p=slot(s,pair);unsigned volume=p[0x17];
        if(s->attenuation)volume=volume*0x70>>8;
        if(level!=255)volume=volume*level>>8;
        for(unsigned channel=pair*2;channel<pair*2+2;channel++){
            select_channel(s,channel);resource_pcm_chip_write(&s->chip,0,volume);
        }
    }
}
bool resource_pcm_driver_tick(resource_pcm_driver *s){
    if(!s)return false;
    s->error=0;
    unsigned mode=s->ram[0x11];
    if(mode!=0 && mode!=1 && mode!=2 && mode!=0x82){s->error=3;return false;}
    if(mode==2 || mode==0x82)speech_poll(s);
    else for(int pair=1;pair>=0;pair--){
        uint8_t *p=slot(s,pair);if(!p[0x14])continue;
        unsigned address=resource_pcm_chip_read(&s->chip,16+pair*4)|(unsigned)resource_pcm_chip_read(&s->chip,17+pair*4)<<8;
        putword(p+0x10,address);
        if((p[0]&2) && (address&16383)<8192){p[0]&=~2;loop_address(s,pair,word(p+0x12));}
        unsigned end=word(p+0x12);
        if(end==address){if(mode==1){s->flags&=~8;stop(s,pair);}}
        else if(!(p[0]&1) && ((end>address && end-address<8192) || (end<address && address-end>=8192)))p[0]|=4;
    }
    speech_reads(s);
    if(!speech_ready(s)){s->error=2;return false;}
    /* Native151D4 uploads queued halves after both cursor tests. */
    for(int pair=1;pair>=0;pair--){
        uint8_t *p=slot(s,pair);
        if(p[0]&4){
            if(!upload(s,pair,word(p+10))){s->error=2;return false;}p[0]&=~4;
            if(pair==0 && (s->flags&2) && !(p[0]&1))history_add(s,longword(p+2));
        }
    }
    channels(s);return true;
}
