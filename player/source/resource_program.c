#include "resource_program.h"
#include <string.h>
static unsigned word(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static bool append(uint8_t out[512],unsigned *used,unsigned value){
    if(*used>508)return false; /* Always leave room for task0. */
    out[(*used)++]=value>>8;out[(*used)++]=value;return true;
}
static unsigned expression_length(const uint8_t *p){
    switch(*p){
    case 0x00:return 6;
    case 0x04:case 0x0a:case 0x0d:case 0x13:case 0x14:
    case 0x3d:case 0x45:case 0x4a:return 3;
    case 0x46:case 0x48:return 1;
    case 0x42:
        if(p[1]==0x24 && word(p+2)==8)return 12;
        if(p[1]==0x22 && (word(p+2)==7 || word(p+2)==8))return 6;
        return 9;
    default:return 0;
    }
}
static void commit_effects(resource_program_state *s,unsigned first,unsigned end){
    /* The whole expression sequence and queue have already been validated.
     * Revisit only its immediate writes, without a large stack/RAM copy of
     * story fields or an arbitrary cap on the number of writes. */
    for(unsigned pc=first;pc<end;){
        const uint8_t *p=s->script+pc+1;
        if(*p==0x13 || *p==0x14){
            unsigned key=word(p+1),at=(key>>7)*8+((key>>4)&7),width=(key&15)+1;
            for(unsigned i=0;i<width;i++){
                unsigned byte=(at+i)/8,bit=7-(at+i)%8;
                if(*p==0x14)s->fields[byte]|=1u<<bit;
                else s->fields[byte]&=~(1u<<bit);
            }
        }else if(*p==0x42 && p[1]==0x24 && word(p+2)==6 && word(p+5)==0xffff){
            *s->pcm_attenuation=(uint8_t)word(p+7);
        }
        pc+=1+expression_length(p);
    }
}
bool resource_program_begin(resource_program_state *s,const void *data,size_t bytes,
    uint16_t pc,uint16_t scene_key,uint16_t word79f0){
    if(!s || !data || !bytes || bytes>65536 || pc>=bytes)return false;
    memset(s,0,sizeof(*s));s->script=data;s->bytes=bytes;s->pc=pc;
    s->scene_key=scene_key;s->word79f0=word79f0;return true;
}
int resource_program_next(resource_program_state *s,resource_tasks_state *q){
    if(!s || !q || !s->script)return -1;
    s->error=0;
    if(q->active){s->error=1;return -1;}
    if(s->dialogue)return 2;
    resource_program_state next=*s;
    uint8_t out[512]={0};unsigned used=0,pc=s->pc;
    if(pc<s->bytes && s->script[pc]==0x30)pc++;
    unsigned first=pc;bool effects=false;
    for(unsigned budget=0;budget<256;budget++){
        if(pc>=s->bytes){s->error=2;return -1;}
        unsigned marker=s->script[pc];
        /* A module/control continuation belongs to the story VM. Leave
         * its original bytes unconsumed, as for the dialogue boundary. */
        if(marker==0x2c){
            if(pc+6>s->bytes){s->error=2;return -1;}
            if(s->script[pc+1]!=0x21){s->error=4;return -1;}
            next.pc=pc;next.dialogue=1;goto finish;
        }
        if(marker==0x2b){
            if(pc+3>s->bytes || word(s->script+pc+1)>=s->bytes){s->error=2;return -1;}
            next.pc=word(s->script+pc+1);
            if(next.script[next.pc]!=0x30){s->error=3;return -1;}
            break;
        }
        if(marker!=0x25 || pc+2>s->bytes){s->error=3;return -1;}
        const uint8_t *p=s->script+pc+1;unsigned op=*p,length=0;
        switch(op){
        case 0x00:length=6;break;
        case 0x04:case 0x0a:case 0x0d:case 0x13:case 0x14:case 0x3d:case 0x45:case 0x4a:length=3;break;
        case 0x46:case 0x48:length=1;break;
        case 0x42:
            if(pc+7>s->bytes){s->error=2;return -1;}
            length=expression_length(p);
            break;
        case 0x43:next.pc=pc;next.dialogue=1;goto finish;
        default:s->error=4;return -1;
        }
        if(pc+1+length>s->bytes){s->error=2;return -1;}
        bool good=true;
#define EMIT(v) do{if(!append(out,&used,(v)))good=false;}while(0)
        switch(op){
        case 0x00:
            if(p[1]!=0x23 || word(p+2)>=64 || word(p+4)>5){s->error=5;return -1;}
            EMIT(9);EMIT(word(p+4));EMIT(word(p+2));break;
        case 0x04:EMIT(13);EMIT(word(p+1));break;
        case 0x0a:EMIT(7);EMIT(word(p+1));break;
        case 0x0d:EMIT(14);EMIT(word(p+1));break;
        case 0x13:case 0x14:{
            unsigned key=word(p+1),at=(key>>7)*8+((key>>4)&7),width=(key&15)+1;
            if(!next.fields || at+width>128*8){s->error=5;return -1;}
            effects=true;break;
        }
        case 0x3d:{
            unsigned key=word(p+1);
            /* 17314 suppresses an identical file AND scene. A changed
             * scene in the same file clears but does not reload the file. */
            if(key!=next.scene_key){
                EMIT(15);
                if((key>>8)!=(next.scene_key>>8)){EMIT(17);EMIT(2);EMIT(0x8000);EMIT(key>>8);}
                EMIT(18);EMIT(key&255);next.scene_key=key;
            }
            break;
        }
        case 0x42:
            if(p[1]==0x24 && word(p+2)==8){
                if(p[4]!=0x1f || word(p+5)!=1 || p[7]!=0x22){s->error=5;return -1;}
                EMIT(21);EMIT(word(p+8));EMIT(word(p+10));
            }else if(p[1]==0x22 && word(p+2)==8){
                if(word(p+4)){s->error=5;return -1;}
                EMIT(9);EMIT(2);EMIT(1);
            }else if(p[1]==0x22 && word(p+2)==7){
                EMIT(29);EMIT(word(p+4)!=0);
            }else{
                /* The entry's custom6 write occurs during expression
                 * evaluation, before the queued resource tasks. */
                if(p[1]!=0x24 || word(p+2)!=6 || p[4]!=0x22){s->error=5;return -1;}
                if(word(p+5)==0x79f0)next.word79f0=word(p+7);
                else if(word(p+5)==0xffff && next.pcm_attenuation)effects=true;
                else{s->error=5;return -1;}
            }
            break;
        case 0x45:if(word(p+1)>=64){s->error=5;return -1;}EMIT(9);EMIT(3);EMIT(word(p+1));break;
        case 0x46:EMIT(10);break;
        case 0x48:EMIT(11);break;
        case 0x4a:EMIT(16);EMIT(word(p+1));break;
        }
#undef EMIT
        if(!good){s->error=6;return -1;}
        pc+=1+length;
        if(budget==255){s->error=6;return -1;}
    }
finish:
    if(!used && next.dialogue){if(effects)commit_effects(&next,first,pc);*s=next;return 2;}
    /* Terminating task0 and all bytes beyond it remain zero. */
    if(!resource_tasks_begin(q,out,used+2)){s->error=6;return -1;}
    if(effects)commit_effects(&next,first,pc);
    *s=next;return 1;
}
