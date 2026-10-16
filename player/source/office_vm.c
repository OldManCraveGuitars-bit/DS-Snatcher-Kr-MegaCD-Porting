#include <stdint.h>
#include <string.h>
#include "office_vm.h"
#include "../generated/office_branch.h"
unsigned office_queue[OFFICE_QUEUE_CAPACITY],office_queue_count;
unsigned office_speakers[OFFICE_QUEUE_CAPACITY];
static uint8_t state_bits[128];
static uint32_t rng;
static unsigned budget;
static bool valid;
static unsigned field_read(unsigned key) {
    unsigned start=(key>>7)*8+((key>>4)&7),width=(key&15)+1,result=0;
    if(start+width>sizeof(state_bits)*8){valid=false;return 0;}
    for(unsigned i=0;i<width;i++)result=(result<<1)|((state_bits[(start+i)/8]>>(7-(start+i)%8))&1);
    return result;
}
static void field_write(unsigned key,unsigned value) {
    unsigned start=(key>>7)*8+((key>>4)&7),width=(key&15)+1;
    if(start+width>sizeof(state_bits)*8){valid=false;return;}
    for(unsigned i=0;i<width;i++){
        unsigned byte=(start+i)/8,bit=7-(start+i)%8;
        state_bits[byte]=(state_bits[byte]&~(1u<<bit))|(((value>>(width-1-i))&1u)<<bit);
    }
}
static bool spend(int id,unsigned depth) {
    if(!valid || !budget || id<0 || id>=OFFICE_NODE_COUNT || depth>64){valid=false;return false;}
    budget--;return true;
}
static void add_choice(int id,int *choices,unsigned *count) {
    if(id<0 || id>=OFFICE_NODE_COUNT){valid=false;return;}
    const OfficeNode *n=&office_nodes[id];unsigned repeat=n->op==0x34?n->arg:1;int child=n->op==0x34?n->left:id;
    if(!repeat || repeat>32-*count){valid=false;return;}
    while(repeat--)choices[(*count)++]=child;
}
static void alternatives(int id,int *choices,unsigned *count,unsigned depth) {
    if(!spend(id,depth))return;
    const OfficeNode *n=&office_nodes[id];
    if(n->op==0x25 || n->op==0x26){add_choice(n->left,choices,count);alternatives(n->right,choices,count,depth+1);}
    else if(n->op==0x27)add_choice(n->left,choices,count);
    else add_choice(id,choices,count);
}
static void execute(int id,unsigned depth) {
    if(!spend(id,depth))return;
    const OfficeNode *n=&office_nodes[id];
    switch(n->op){
    case 0x25:case 0x26:execute(n->left,depth+1);execute(n->right,depth+1);break;
    case 0x0c:case 0x27:case 0x30:case 0x34:execute(n->left,depth+1);break;
    case 0x2d:break;
    case 0x1b:if(field_read(n->arg))execute(n->left,depth+1);break;
    case 0x19:execute(field_read(n->arg)?n->left:n->right,depth+1);break;
    case 0x3e:case 0x3f:{
        int choices[32];unsigned count=0;alternatives(n->left,choices,&count,depth+1);
        if(!valid || !count){valid=false;break;}
        unsigned choice;
        if(n->op==0x3f)choice=field_read(n->arg);
        else{rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;choice=rng%count;}
        if(choice>=count){valid=false;break;}
        execute(choices[choice],depth+1);
        if(n->op==0x3e || choice+1<count)field_write(n->arg,choice+1);
        break;
    }
    case 0x43:
        if(office_queue_count==OFFICE_QUEUE_CAPACITY || n->arg>=OFFICE_TEXT_COUNT || n->speaker>=49){valid=false;break;}
        office_queue[office_queue_count]=n->arg;office_speakers[office_queue_count++]=n->speaker;break;
    default:valid=false;break;
    }
}
void office_reset(void){memset(state_bits,0,sizeof(state_bits));rng=0x534e4453;office_queue_count=0;}
bool office_choose(unsigned option){office_queue_count=0;budget=512;valid=option<OFFICE_MENU_COUNT;if(valid)execute(office_menu_actions[option],0);return valid;}
void office_export(uint8_t out[OFFICE_STATE_BYTES]){
    memcpy(out,state_bits,128);for(unsigned i=0;i<4;i++)out[128+i]=(uint8_t)(rng>>(i*8));
}
void office_import(const uint8_t in[OFFICE_STATE_BYTES]){
    memcpy(state_bits,in,128);rng=0;for(unsigned i=0;i<4;i++)rng|=(uint32_t)in[128+i]<<(i*8);
}
