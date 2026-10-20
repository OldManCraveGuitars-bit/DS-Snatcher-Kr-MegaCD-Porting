#include "resource_practice.h"
#include "resource_actor.h"
#include <string.h>
static uint8_t *memory(resource_practice *p,unsigned at,unsigned size){
    if(at>=0x7800 && at+size<=0x7a00)return p->ram+at-0x7800;
    if(at>=0x7a00 && at+size<=0x8e00)return p->actors+at-0x7a00;
    p->error=1;return NULL;
}
static unsigned byte(resource_practice *p,unsigned at){uint8_t *v=memory(p,at,1);return v?*v:0;}
static unsigned word(resource_practice *p,unsigned at){uint8_t *v=memory(p,at,2);return v?((unsigned)v[0]<<8)|v[1]:0;}
static uint32_t longword(resource_practice *p,unsigned at){return (uint32_t)word(p,at)<<16|word(p,at+2);}
static void putbyte(resource_practice *p,unsigned at,unsigned v){uint8_t *d=memory(p,at,1);if(d)*d=v;}
static void putword(resource_practice *p,unsigned at,unsigned v){uint8_t *d=memory(p,at,2);if(d){d[0]=v>>8;d[1]=v;}}
static void putlong(resource_practice *p,unsigned at,uint32_t v){putword(p,at,v>>16);putword(p,at+2,v);}
static unsigned addword(resource_practice *p,unsigned at,int delta){unsigned v=(word(p,at)+delta)&65535;putword(p,at,v);return v;}
static unsigned original(resource_practice *p,uint32_t at){
    /* A source actor can delete itself at the end of its death sequence.
     * The next controller pass reads the cleared pointer before completion.
     * Model its actual low RAM read, preserving strict unknown-address errors. */
    if(at<2)return at?(p->zero_word&255):(p->zero_word>>8);
    if(at>=0x28000 && at-0x28000<p->resource_bytes)return p->resource[at-0x28000];
    if(at>=0xd400 && at-0xd400<p->subcode_bytes)return p->subcode[at-0xd400];
    p->error=2;return 0;
}
static unsigned original_word(resource_practice *p,uint32_t at){return original(p,at)<<8|original(p,at+1);}
static unsigned stage(resource_practice *p){return word(p,0x78e0);}
static unsigned table(resource_practice *p,unsigned at){return original(p,at+stage(p));}
static bool practice(resource_practice *p){return stage(p)==0 || stage(p)==11;}
static unsigned kill_goal(resource_practice *p){return table(p,word(p,0x7972)?0x11f90:0x11f80);}
static unsigned max_targets(resource_practice *p){return table(p,word(p,0x7972)?0x11fc0:0x11fb0);}
static void request(resource_practice *p,unsigned entry,unsigned arg,unsigned channel){
    if(!p->request || !p->request(p->context,entry,arg,channel))p->error=3;
}
static unsigned random_word(resource_practice *p){
    uint32_t state=longword(p,0x786a);unsigned value=resource_actor_random(&state);
    putlong(p,0x786a,state);return value;
}
static unsigned random_cell(resource_practice *p){return original(p,0x12010+(random_word(p)&31));}
static unsigned marker(resource_practice *p,unsigned actor,unsigned index){return original(p,longword(p,actor+0x32)+index);}
static void faster(resource_practice *p){if(word(p,0x78e2)>=2)addword(p,0x78e2,-2);}
static void slower(resource_practice *p){
    unsigned limit=!practice(p) && word(p,0x7972)?48:32;
    if(word(p,0x78e2)<limit)addword(p,0x78e2,1);
}
static void advance_target(resource_practice *p,unsigned at,unsigned index){
    unsigned m=marker(p,at,index);
    if(m!=word(p,at+0x28))return;
    putword(p,at+0x28,m+2);putbyte(p,at+0x24,byte(p,at+0x24)+1);
    putword(p,at+0x2c,2*random_cell(p)+word(p,0x78e2));
}
static void force_target(resource_practice *p,unsigned at,unsigned phase){
    putbyte(p,at+0x24,(byte(p,at+0x24)&0xf0)+phase-1);
    putword(p,at+0x28,marker(p,at,phase-1)+2);
    putbyte(p,at+0x24,byte(p,at+0x24)+1);
    putword(p,at+0x2c,2*random_cell(p)+word(p,0x78e2));
}
static void targets(resource_practice *p){ /*12884..12B2C*/
    putword(p,0x78ec,0);
    for(unsigned at=0x8400;at<0x86d0;at+=80){
        unsigned type=byte(p,at+0x24),phase=type==255?0:type&15;
        if(phase)addword(p,0x78ec,1);
        if(phase==0){
            unsigned m=marker(p,at,0);
            if(m==2)putbyte(p,at+0x25,3);
            else if(m>=128 || type==255){putbyte(p,at+0x24,255);putbyte(p,at+0x25,3);putword(p,at,0);}
            else putword(p,at,1);
        }else if(phase==1){putbyte(p,at+0x25,0);advance_target(p,at,1);}
        else if(phase<=4){
            if((int16_t)addword(p,at+0x2c,-1)<0){
                advance_target(p,at,phase);
                if((byte(p,at+0x24)&15)==phase)putword(p,at+0x2c,0);
                else if(table(p,0x11fd0)){
                    if(phase==2 && kill_goal(p)<word(p,0x790a))force_target(p,at,5);
                    else if(phase==3 && (byte(p,at+0x24)&0xf0)!=0x20 &&
                            !word(p,0x78fe) && !word(p,0x7946) && word(p,0x78f8)<2){
                        putword(p,0x791a,(at-0x8400)/80);putword(p,0x78fe,1);
                    }else if(phase==4){
                        if((table(p,0x11fd0)&128) ||
                           (kill_goal(p)<word(p,0x790a) && random_cell(p)<6))force_target(p,at,3);
                    }
                }
            }
        }else if(phase==5){
            advance_target(p,at,5);
            if((byte(p,at+0x24)&15)!=5){
                addword(p,at+0x28,-2);
                if(!table(p,0x11fd0) && byte(p,at+0x24)>=0x30)addword(p,0x7910,1);
                putbyte(p,at+0x24,0);
            }
        }else if(phase==6){
            putbyte(p,at+0x24,(type&0xf0)+6);putword(p,at+0x28,marker(p,at,6));
            putbyte(p,at+0x24,byte(p,at+0x24)+1);
            putword(p,at+0x2c,2*random_cell(p)+word(p,0x78e2));
        }else if(phase==7){
            advance_target(p,at,7);
            if((byte(p,at+0x24)&15)==8){
                addword(p,at+0x28,-2);putbyte(p,at+0x24,0);
                if(!practice(p))putbyte(p,at+0x25,3);
            }
        }else p->error=4;
    }
}
static void spawn(resource_practice *p){ /*12B2E..12D98*/
    unsigned phase=word(p,0x78fc);
    if(phase==0){
        if((int16_t)addword(p,0x7944,-1)>=0)return;
        unsigned goal=kill_goal(p),limit=table(p,0x11fa0),max=max_targets(p);
        if(goal && goal<=word(p,0x790a))return;
        if(limit && limit==word(p,0x7906))return;
        if(max==word(p,0x78ec))return;
        putword(p,0x7944,2*random_cell(p)+16+word(p,0x78e2));
        if(random_cell(p)>=7)putword(p,0x7944,5);
        putword(p,0x78fc,1);
    }else if(phase==1){
        unsigned cell=random_cell(p);if(cell>=9){p->error=5;return;}
        unsigned at=0x8400+cell*80;putlong(p,0x791c,at);
        unsigned type=byte(p,at+0x24);if(type==255 || (type&15))return;
        putword(p,0x78fc,2);addword(p,0x7906,1);
        type=1;
        if(practice(p)){
            unsigned selection=random_cell(p);
            type=original(p,0x12c9a+(stage(p)==11?selection:selection&3));
            if(type<1 || type>(stage(p)==11?3u:2u)){p->error=6;return;}
            /* Original12C82 uses all nine random entries and model type+1,
             * while12C66 uses its first four entries and model type-1. */
            unsigned model=0x8220+(stage(p)==11?type+1:type-1)*80;
            putbyte(p,at+0x16,byte(p,model+0x16));putword(p,at+0x2a,word(p,model+0x2a));
            for(unsigned pos=0x2e;pos<=0x36;pos+=4)putlong(p,at+pos,longword(p,model+pos));
            putword(p,at+0x28,2);
        }else if(stage(p)==2){
            /* Source12CA8/12CF0 alternates Freddy models and upper/lower targets. */
            unsigned model_index=(cell<3?1u:0u)|word(p,0x7912);
            putword(p,0x7912,(model_index&2)^2);
            unsigned model=0x8220+model_index*80;
            putbyte(p,at+0x16,byte(p,model+0x16));putword(p,at+0x2a,word(p,model+0x2a));
            for(unsigned pos=0x2e;pos<=0x36;pos+=4)putlong(p,at+pos,longword(p,model+pos));
            type=(model_index<<1)|1;
        }
        if(stage(p)==4){ /* Original12CD2 uses model0 for the mirror target. */
            unsigned model=0x8220;
            putbyte(p,at+0x16,byte(p,model+0x16));putword(p,at+0x2a,word(p,model+0x2a));
            for(unsigned pos=0x2e;pos<=0x36;pos+=4)putlong(p,at+pos,longword(p,model+pos));
        }
        /* Original12CDA selects encounter10 type from its nine-cell table. */
        if(stage(p)==10)type=original(p,0x12ce2+cell);
        putbyte(p,at+0x24,type<<4);
        if(type==2){addword(p,0x7914,1);addword(p,0x7906,-1);}
    }else if(phase==2){
        unsigned at=longword(p,0x791c);
        if(at<0x8400 || at>=0x86d0 || (at-0x8400)%80){p->error=7;return;}
        unsigned m=marker(p,at,0);putword(p,at+0x28,m==2?m:m+2);putword(p,at,1);
        if(practice(p) && byte(p,at+0x24)!=0x20){putword(p,0x8c20,1);putbyte(p,0x8c45,1);addword(p,0x8c48,1);}
        putbyte(p,at+0x24,(byte(p,at+0x24)&0xf0)|1);putword(p,0x78fc,0);
        if(practice(p))request(p,0x150fa,0x22,0);
    }else p->error=8;
}
static void lightgun_aim(resource_practice *p){
    unsigned x=byte(p,0x798a),y=byte(p,0x798b);
    putword(p,0x7900,(x<0x60?0:x<0xa0?1:2)+(y<0x30?0:y<0x58?3:6));
}
static void input(resource_practice *p){ /*12400..125F0*/
    unsigned weapon=word(p,0x78f8);
    if(weapon>=2){
        if(weapon&2){
            int y=(random_word(p)&15)-8;putword(p,0x8136,y);addword(p,0x80ea,y);
            int x=(random_word(p)&15)-8;putword(p,0x813a,x);addword(p,0x80e6,x);
            putword(p,0x7946,(word(p,0x78e2)>>2)+24);
        }
        addword(p,0x78f8,-2);return;
    }
    putword(p,0x813a,0);putword(p,0x8136,0);
    if(word(p,0x78f2) || (int16_t)word(p,0x78e8)>=4)return;
    unsigned gun=word(p,0x7972),pressed=byte(p,gun?0x7985:0x7991);
    if(pressed&64){
        unsigned drawn=0;putword(p,0x7970,0);weapon^=1;putword(p,0x78f8,weapon);
        if(weapon){request(p,0x150ee,table(p,0x11f20),255);drawn=1;putword(p,0x7918,1);}
        if(weapon && gun){putword(p,0x7970,1);putword(p,0x80e0,0);}
        else{putword(p,0x80e0,drawn);putword(p,0x7900,4);putword(p,0x80e6,112);putword(p,0x80ea,56);}
        putword(p,0x8130,drawn);
    }
    if(!word(p,0x78f8) || gun || word(p,0x78ee))return;
    unsigned held=byte(p,0x7990),x=0,y=0,cell=4;
    if(held&8){x+=2;cell++;}
    if(held&4)cell--;else x+=2;
    if(held&2){y+=2;cell+=3;}
    if(held&1)cell-=3;else y+=2;
    putword(p,0x80e6,original_word(p,0x11fe0+x));putword(p,0x80ea,original_word(p,0x11fe6+y));
    putword(p,0x7900,cell);
}
static void shot_start(resource_practice *p){
    unsigned at=0x8090,cell=stage(p)==14?4:byte(p,0x7901);if(cell>=9){p->error=9;return;}
    putword(p,at,1);p->scratch_word_2a=2;putword(p,at+0x28,0);putbyte(p,at+0x17,cell);
    putword(p,at+6,original_word(p,0x11fec + cell*2));putword(p,at+10,original_word(p,0x11ffe + cell*2));
    putword(p,0x78fa,2);request(p,0x150ee,table(p,0x11f30),255);
}
static void shot_finish(resource_practice *p){
    putbyte(p,0x80a7,255);putword(p,0x8090,0);putword(p,0x78fa,0);
}
static void shot(resource_practice *p){ /*125F2..12860*/
    unsigned at=0x8090,gun=word(p,0x7972);
    if(gun && word(p,at+0x2c)){
        if((int16_t)addword(p,at+0x2c,1)>=2){putword(p,at+0x2c,0);lightgun_aim(p);}
    }
    switch(word(p,0x78fa)){
    case 0:{
        if(word(p,at) || byte(p,at+0x17)!=255 || word(p,0x78e8)==4 || word(p,0x78f8)!=1)return;
        unsigned pressed=byte(p,gun?0x7985:0x7991);
        if((pressed&64) || !(pressed&32))return;
        if(stage(p)==14)putword(p,0x78ee,1); /* Original12672: lock the decision aim. */
        addword(p,0x7908,1);putword(p,0x78fa,4);if(gun)putword(p,at+0x2c,1);return;
    }
    case 4:if(word(p,0x78e2)>=16){putword(p,0x78fa,5);return;} /*fall through*/
    case 5:putword(p,0x78fa,1);if(word(p,0x78e2)>=4)return; /*fall through*/
    case 1:shot_start(p);return;
    case 2:
        if(word(p,at+0x28)>=4){
            if(stage(p)==14){ /* Original1271A: firing selects the source790C branch. */
                putword(p,0x790c,1);putword(p,0x78e8,3);putword(p,0x78ea,0);
                request(p,0x150ee,0x47,255);
            }else putword(p,0x78fa,3);
        }return;
    case 3:{
        unsigned cell=byte(p,at+0x17);if(cell>=9){p->error=10;return;}
        unsigned target=0x8400+80*cell,type=byte(p,target+0x24),phase=type&15;
        if(phase>=2 && phase<5){
            putbyte(p,target+0x24,(type&0xf0)|6);
            if(!(type&16)){
                addword(p,0x790c,1);faster(p);if(!table(p,0x11fd0))request(p,0x150ee,14,255);
            }
            else{
                if((word(p,0x790a)&7)==7)faster(p);
                addword(p,0x790a,1);
                if(practice(p)){putword(p,0x8c70,1);putbyte(p,0x8c95,1);addword(p,0x8c98,1);}
                request(p,0x150ee,table(p,0x11f10),255);
            }
            /* Original1280E locks aim after an encounter9 hit. */
            if(stage(p)==9){putword(p,0x7970,0);putword(p,0x78f2,1);}
        }else if(stage(p)!=10){
            if(gun || !table(p,0x11fd0))slower(p);
            if(!addword(p,0x790e,1))putword(p,0x790e,255);
        }
        if(stage(p)==10){
            /* Original12862 tests the authored cell table after both hit and
             * miss. Some cells inflict damage instead of counting a miss. */
            if(original(p,0x12ce2+cell)&2){
                request(p,0x150ee,50,255);request(p,0x150fa,19,0);
                if(word(p,0x7910)<6){
                    unsigned life=addword(p,0x892c,-1);
                    p->health_vram=original_word(p,0x12ebc+(int16_t)(life*2));
                    request(p,0xe81a,0x12ea8,0);addword(p,0x7910,1);
                }
            }else if(!addword(p,0x790e,1))putword(p,0x790e,255);
        }
        shot_finish(p);return;
    }
    default:p->error=11;
    }
}
static void damage(resource_practice *p){ /*12D9A..12EA6*/
    if(!table(p,0x11fd0))return;
    unsigned at=0x8180;
    switch(word(p,0x78fe)){
    case 0:
        putword(p,at,0);putword(p,at+0x28,0);
        if(word(p,0x7946))addword(p,0x7946,-1);
        return;
    case 1:{
        unsigned cell=word(p,0x791a);if(cell>=9){p->error=19;return;}
        putword(p,at+6,word(p,0x7920+2*cell));putword(p,at+10,word(p,0x7932+2*cell));
        putword(p,at,1);putword(p,at+0x28,2);putword(p,0x78fe,2);
        unsigned bank=table(p,0x11f40);if(bank)request(p,0x150ee,bank,255);return;
    }
    case 2:if(marker(p,at,0)==word(p,at+0x28))putword(p,0x78fe,3);return;
    case 3:
        addword(p,0x78f8,0x38);putword(p,0x78fe,0);putword(p,at,0);
        if(stage(p)==4)request(p,0x150ee,52,255);else request(p,0x150fa,19,0);
        if(word(p,0x7910)<6){
            unsigned life=addword(p,0x892c,-1);
            p->health_vram=original_word(p,0x12ebc+2*life);
            request(p,0xe81a,0x12ea8,0);addword(p,0x7910,1);
        }return;
    default:p->error=20;
    }
}
static void completed(resource_practice *p){ /*12EC8..12FF4*/
    if(table(p,0x11fd0) && word(p,0x7910)>=6){
        for(unsigned at=0x8400;at<0x86d0;at+=80)putbyte(p,at+0x25,byte(p,at+0x25)|1);
        request(p,0xe4a0,0,0);putword(p,0x78e8,3);putword(p,0x7910,6);return;
    }
    unsigned goal=kill_goal(p),limit=table(p,0x11fa0);
    if(goal && goal>word(p,0x790a))return;
    if(limit && (word(p,0x78fc) || limit>word(p,0x7906)))return;
    unsigned max=max_targets(p);
    if(max!=1 && word(p,0x78fe))return;
    if(word(p,0x78fa))return;
    for(unsigned at=0x8400;at<0x86d0;at+=80){
        unsigned type=byte(p,at+0x24);if(type==255)continue;if(type&15)return;
        /* Original12F88 skips this clear only for encounter7. */
        if(table(p,0x11fd0) && stage(p)!=7)putword(p,at,0);
    }
    if(practice(p)){putword(p,0x8b80,1);putword(p,0x8bd0,1);putword(p,0x8ba8,2);putword(p,0x8bf8,2);}
    putword(p,0x78e8,3);
}
static void initialize(resource_practice *p){ /*12168..1228E, stages0/2/3*/
    putword(p,0x8900,0);putbyte(p,0x8925,3);putword(p,0x892c,6);
    for(unsigned at=0x78f8;at<0x791c;at+=2)putword(p,at,0);
    putword(p,0x8090,0);putbyte(p,0x80b5,0);putbyte(p,0x80a7,255);
    putword(p,0x8180,0);putbyte(p,0x81a5,0);putword(p,0x80e0,0);putbyte(p,0x8105,0);
    putword(p,0x8130,0);putbyte(p,0x8155,0);putword(p,0x78e6,0);putword(p,0x78f2,0);
    for(unsigned at=0x8400;at<0x86d0;at+=80){putword(p,at+0x28,2);putword(p,at+0x2a,0);putbyte(p,at+0x24,0);}
    /* Source12248 DBRA repeatedly clears the SAME word: no postincrement. */
    if(table(p,0x11fd0))putword(p,0x8400,0);
    for(unsigned i=0;i<36;i+=2)putword(p,0x7920+i,original_word(p,0x11fec+i));
    /* Source12290 adjusts the two Freddy hit positions. */
    if(stage(p)==2){putword(p,0x7922,0x48);putword(p,0x7934,8);putword(p,0x7928,0x48);putword(p,0x793a,0x23);}

    /* Original122AA/122B8: Queens encounter-specific hit coordinates. */
    if(stage(p)==5){putword(p,0x7922,0x5d);putword(p,0x7934,0x36);}
    if(stage(p)==6){putword(p,0x792c,0x26);putword(p,0x793e,0x48);}
    /* Original122C6 adjusts encounter7 only; encounter8 uses the base table. */
    if(stage(p)==7){putword(p,0x792a,0x96);putword(p,0x793c,0x30);}
    /* Original SUB122D4/122E2: encounters9/10 retain their own hit positions. */
    if(stage(p)==9){putword(p,0x7922,0x60);putword(p,0x7934,0);}
    if(stage(p)==10){putword(p,0x7932,0);putword(p,0x7936,0);putword(p,0x793e,0x58);putword(p,0x7942,0x58);}
    /* Original122FC: encounter13 uses its own authored damage positions. */
    if(stage(p)==13){
        putword(p,0x7932,0x18);putword(p,0x7936,0x18);putword(p,0x7938,0x28);
        putword(p,0x7928,0x6e);putword(p,0x793a,0x27);putword(p,0x793c,0x28);
    }
    putword(p,0x7944,table(p,0x11f60));putword(p,0x78e8,table(p,0x11f50));putword(p,0x78ea,0);
    if(!word(p,0x78e2))putword(p,0x78e2,table(p,0x11f70));
}
static void begin_phase(resource_practice *p){
    switch(word(p,0x78ea)){
    case 0:addword(p,0x78ea,1);p->host_request|=32;putword(p,0x78e4,0);request(p,0xe41a,0x11e7e,0);return;
    case 1:
        if((int16_t)addword(p,0x78e4,1)<2 || !(p->host_ready&32))return;
        p->host_request&=~32;putword(p,0x78e4,0);request(p,0xe41a,0x11e6a,0);
        if(practice(p))addword(p,0x78ea,2);
        else{
            request(p,0xe81a,0x11ee6,0);
            if(stage(p)!=5){request(p,0xe81a,0x11ed8,0);request(p,0xe81a,0x11ef4,0);}
            addword(p,0x78ea,1);
        }return;
    case 2:putword(p,0x7902,p->host_x);putword(p,0x7904,p->host_y);addword(p,0x78ea,1);return;
    case 3:initialize(p);return;
    default:p->error=12;
    }
}
static void ready(resource_practice *p){
    input(p);
    switch(word(p,0x78fa)){
    case 0:
        putbyte(p,0x8ba5,1);putbyte(p,0x8bf5,1);putword(p,0x8ba8,1);putword(p,0x8bf8,1);
        putword(p,0x7946,128);putword(p,0x78fa,1);return;
    case 1:{
        unsigned remaining=word(p,0x7946);
        if(!remaining){putword(p,0x78fa,2);return;}
        if(!(remaining&63))request(p,0x150fa,17,0);
        addword(p,0x7946,-1);return;
    }
    case 2:putword(p,0x8b80,0);putword(p,0x8bd0,0);request(p,0x150fa,18,0);putword(p,0x78fa,0);putword(p,0x78e8,2);return;
    default:p->error=13;
    }
}
static void finish(resource_practice *p){
    if(word(p,0x7914) && word(p,0x7914)==word(p,0x790c))putword(p,0x7916,1);
    if(!practice(p))request(p,0xe41a,word(p,0x7910)>=6?0x11e8c:0x11e92,0);
    putword(p,0x8130,0);putword(p,0x80e0,0);putword(p,0x7970,0);putword(p,0x78e8,4);putword(p,0x78e4,0);
    request(p,0x150fa,255,0);
}
static void close_phase(resource_practice *p){
    switch(word(p,0x78ea)){
    case 0:
        if(p->pcm_flags&8)return;
        if(max_targets(p)==1 || (int16_t)addword(p,0x78e4,1)>=64){addword(p,0x78ea,1);return;}
        if(word(p,0x7910)<6){input(p);shot(p);}return;
    case 1:addword(p,0x78ea,1);putword(p,0x8090,0);request(p,0xe4a0,0,0);return;
    case 2:addword(p,0x78ea,1);putword(p,0x78e4,0);request(p,0xe41a,0x11e7e,0);return;
    case 3:
        if(!practice(p)){
            if((int16_t)addword(p,0x78e4,1)<2)return;
            putword(p,0x78e4,0);request(p,0xe81a,0x11f02,0);
        }
        addword(p,0x78ea,1);return;
    case 4:
        if((int16_t)addword(p,0x78e4,1)<2)return;
        putword(p,0x78e4,0);putword(p,0x78e8,5);putword(p,0x78ea,0);request(p,0xe41a,0x11e74,0);return;
    default:p->error=14;
    }
}
static void active_phase(resource_practice *p){
    input(p);shot(p);
    if(stage(p)==14){ /* Original123BA..123FE skips targets/spawn/damage/completion. */
        if(word(p,0x78ee)){putword(p,0x80e6,112);putword(p,0x80ea,56);}
        unsigned ticks=addword(p,0x78e6,1);
        /* Original CMP.W/BMI tests N, including word wrap. */
        if(!((ticks-256)&0x8000)){putword(p,0x78e8,3);putword(p,0x78ea,0);}
    }else{targets(p);spawn(p);damage(p);completed(p);}
}
bool resource_practice_bind(resource_practice *p,uint8_t actors[5120],const void *resource,size_t bytes,const void *sub,size_t subbytes){
    if(!p || !actors || !resource || bytes<8 || !sub || subbytes<0x13150-0xd400)return false;
    memset(p,0,sizeof(*p));p->actors=actors;p->resource=resource;p->resource_bytes=bytes;p->subcode=sub;p->subcode_bytes=subbytes;
    p->health_vram=original_word(p,0x12eae);return true;
}
bool resource_practice_call(resource_practice *p,uint32_t entry){
    if(!p || !p->actors || p->error)return false;
    if(stage(p)!=0 && stage(p)!=2 && stage(p)!=3 && stage(p)!=4 && stage(p)!=5 && stage(p)!=6 && stage(p)!=7 && stage(p)!=8 && stage(p)!=9 && stage(p)!=10 && stage(p)!=11 && stage(p)!=12 && stage(p)!=13 && stage(p)!=14){p->error=15;return false;}
    switch(entry){
    case 0x12168:initialize(p);break;
    case 0x12400:input(p);break;
    case 0x125f2:shot(p);break;
    case 0x12884:targets(p);break;
    case 0x12b2e:spawn(p);break;
    case 0x12d9a:damage(p);break;
    case 0x12ec8:completed(p);break;
    case 0x12ff6:finish(p);break;
    case 0x12322:ready(p);break;
    case 0x13052:close_phase(p);break;
    case 0x120b2:begin_phase(p);break;
    case 0x123b2:active_phase(p);break;
    case 0x1208e:
        switch(word(p,0x78e8)){
        case 0:begin_phase(p);break;case 1:ready(p);break;
        case 2:active_phase(p);break;
        case 3:finish(p);break;case 4:close_phase(p);break;
        case 5:
            if((int16_t)addword(p,0x78e4,1)>=2){
                putword(p,0x78f8,0);putword(p,0x78e8,0);putword(p,0x78e2,0);putword(p,0x7970,0);putbyte(p,0x78af,0);
            }break;
        default:p->error=16;
        }break;
    default:p->error=17;
    }
    return !p->error;
}

bool resource_practice_score_reset(resource_practice *p,bool *complete){
    /* Original13150..131D8, task1F: animate both score gauges down on
     * even source frames, then restore the READY actors. */
    if(!p || !complete || !p->actors || p->error)return false;
    *complete=false;
    if(word(p,0x7848)&1)return true;
    switch(word(p,0x78e8)){
    case 0:{
        int16_t value=(int16_t)word(p,0x8c48);
        if(value<=0){addword(p,0x78e8,1);break;}
        value--;putword(p,0x8c48,(uint16_t)value);
        /* Source CMP.W/BPL tests N alone, including word overflow. */
        if(((unsigned)(uint16_t)value-word(p,0x8c98))&0x8000)addword(p,0x8c98,-1);
        break;
    }
    case 1:
        putbyte(p,0x8c45,3);putbyte(p,0x8c95,3);
        putword(p,0x8c48,0xffff);putword(p,0x8c98,0xffff);
        putword(p,0x8ba8,0);putword(p,0x8bf8,0);addword(p,0x78e8,1);break;
    case 2:putword(p,0x78e8,0);*complete=true;break;
    default:p->error=18;
    }
    return !p->error;
}
