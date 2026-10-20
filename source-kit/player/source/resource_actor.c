#include "resource_random.h"
#include "resource_actor.h"
#include <string.h>

enum { ACTOR_BYTES = 80, ACTORS = 64, COMMAND_LIMIT = 512 };
static uint16_t word(const uint8_t *p) { return (uint16_t)p[0] << 8 | p[1]; }
static uint32_t longword(const uint8_t *p) { return (uint32_t)word(p) << 16 | word(p+2); }
static void put_word(uint8_t *p, uint16_t v) { p[0]=v>>8; p[1]=v; }
static void put_long(uint8_t *p, uint32_t v) { put_word(p,v>>16); put_word(p+2,v); }

static bool control_tree(uint8_t *actors,unsigned id,unsigned op,uint64_t *visited){
    if(id>=ACTORS || (*visited&(UINT64_C(1)<<id)))return false;
    *visited|=UINT64_C(1)<<id;uint8_t *a=actors+id*ACTOR_BYTES;
    switch(op){
    case 0:a[0x25]&=2;break;
    case 1:a[0x25]|=1;break;
    case 2:a[0x25]=3;a[0x4e]=0;break;
    case 5:a[0x25]=a[0x4e]=0;put_word(a+0x28,65535);put_word(a+0x2a,65535);break;
    default:return false;
    }
    unsigned child=word(a+0x4a);
    while(child){
        if(!control_tree(actors,child,op,visited))return false;
        child=word(actors+child*ACTOR_BYTES+0x4c);
    }
    return true;
}
bool resource_actor_control(uint8_t actors[64*80],unsigned id,unsigned op,bool *complete){
    if(!actors || !complete || id>=ACTORS || op>5)return false;
    uint8_t *a=actors+id*ACTOR_BYTES;*complete=true;
    if(op==3){
        if(word(a) && !(a[0x4e]&2))*complete=false;
        else if(word(a))a[0x4e]&=~2;
        return true;
    }
    if(op==4){a[0x4e]=255;return true;}
    uint64_t visited=0;return control_tree(actors,id,op,&visited);
}

uint16_t resource_actor_random(uint32_t *state){return resource_random_next(state);}

typedef struct {
    uint8_t *actors;
    const uint8_t *data;
    size_t size;
    uint32_t base;
    resource_actor_clock *clock;
    resource_actor_effects *effects;
    unsigned id;
} vm;
static bool fail(vm *v, unsigned reason, unsigned command) {
    v->clock->error=reason;v->clock->error_actor=v->id;
    v->clock->error_command=command;return false;
}
static const uint8_t *address(vm *v, uint32_t at, size_t bytes) {
    if(at>=v->base && at-v->base<=v->size && bytes<=v->size-(at-v->base))return v->data+(at-v->base);
    return v->effects && v->effects->resolve?v->effects->resolve(v->effects->context,at,bytes):NULL;
}
static uint8_t *actor(vm *v, unsigned id) {
    return id<ACTORS?v->actors+id*ACTOR_BYTES:NULL;
}
static bool related(vm *v,uint8_t *a,unsigned operation,uint16_t delta,uint64_t *seen) {
    unsigned id=(unsigned)(a-v->actors)/ACTOR_BYTES;
    if(id>=ACTORS || (*seen&(UINT64_C(1)<<id)))return fail(v,3,0);
    *seen|=UINT64_C(1)<<id;
    if(operation==0)a[0x25]&=2;
    else if(operation==1)a[0x25]|=1;
    else if(operation==2 || operation==3){
        unsigned at=operation==2?6:10;put_word(a+at,word(a+at)+delta);
    }else{
        unsigned local=operation==4?0xe:0x12,global=operation==4?0x40:0x44;
        unsigned parent=word(a+0x48);uint32_t velocity=longword(a+local);
        if(parent){uint8_t *p=actor(v,parent);if(!p)return fail(v,3,0);velocity+=longword(p+global);}
        put_long(a+global,velocity);
    }
    /* FEB2's FEDA dispatch calls the same helper for each child, which in
     * turn traverses its descendants. Siblings belong to the parent loop. */
    unsigned child=word(a+0x4a);
    while(child){
        uint8_t *p=actor(v,child);
        if(!p)return fail(v,3,0);
        if(!related(v,p,operation,delta,seen))return false;
        child=word(p+0x4c);
    }
    return true;
}
static bool group(vm *v,uint8_t *a,unsigned operation){
    uint64_t seen=0;return related(v,a,operation,0,&seen);
}
static const uint8_t *command_at(vm *v, uint8_t *a) {
    /* The native index is signed 16-bit after multiplying by four. */
    int32_t displacement=(int16_t)(word(a+0x28)*4);
    uint32_t pointer=longword(a+0x32)+displacement;
    return address(v,pointer,4);
}
static void next(uint8_t *a) { put_word(a+0x28,word(a+0x28)+1); }
static unsigned random_range(vm *v, unsigned low, unsigned extent) {
    return low+((uint32_t)resource_actor_random(&v->clock->rng)*extent>>16);
}
static bool step(vm *v, uint8_t *a) {
    int wait=(int16_t)(word(a+0x2a)-1);
    put_word(a+0x2a,wait);
    if(wait>0) {
        const uint8_t *c=command_at(v,a);
        if(!c)return fail(v,1,0);
        if(c[0]!=0x97)a[0x25]&=~2;
        return true;
    }
    if(!a[0x4f])next(a);
    for(unsigned budget=0;budget<COMMAND_LIMIT;budget++) {
        a[0x25]&=~2;
        const uint8_t *c=command_at(v,a);
        if(!c)return fail(v,1,0);
        unsigned op=c[0],arg=c[1],value=word(c+2);
        v->clock->commands++;
        if(op<0x80 || op==0x97) {
            if(op==0x97)a[0x25]|=2;
            put_word(a+0x2a,arg?random_range(v,c[2],(uint16_t)(c[3]-c[2])):value);
            return true;
        }
        switch(op) {
        case 0x80: memset(a,0,ACTOR_BYTES);return true;
        case 0x81: put_word(a+0x28,arg);continue;
        case 0x82:
            if(arg==a[0x27])a[0x27]=0;
            else { a[0x27]++;put_word(a+0x28,c[3]);continue; }
            break;
        case 0x83:
            if(!a[0x27])a[0x27]=random_range(v,2,(uint8_t)(c[2]-arg));
            if(c[2]==a[0x27])a[0x27]=0;
            else { a[0x27]++;put_word(a+0x28,c[3]);continue; }
            break;
        case 0x84: case 0x85: {
            unsigned at=op==0x84?6:10;uint16_t delta=value-word(a+at);uint64_t seen=0;
            a[0x2a]=255;
            if(!related(v,a,op==0x84?2:3,delta,&seen))return false;
            break;
        }
        case 0x86: case 0x87: {
            unsigned at=op==0x86?0xe:0x12;uint64_t seen=0;
            put_long(a+at,(uint32_t)value<<16|(uint32_t)arg<<8);
            if(!related(v,a,op==0x86?4:5,0,&seen))return false;
            break;
        }
        case 0x8b: case 0x9e:break; /* Native FA88/FC08 are no-ops. */
        case 0x88:
            if(v->effects && v->effects->palette_blocked)break;
            /* Palette/glyph/scroll/scene hooks use the native sequence base,
             * rather than an observed timeline or inferred sprite motion. */
            /* fall through */
        case 0x89: case 0x8a: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
        case 0x9d: case 0x9f: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6:
        case 0xa8: case 0xaa: case 0xab: case 0xad: case 0xae:
            if(!v->effects || !v->effects->command ||
               !v->effects->command(v->effects->context,v->id,op,arg,value,longword(a+0x32)))return fail(v,2,op);
            if(op==0xa2 && a[0x4f])return true;
            break;
        case 0x90: case 0x91: case 0x92: case 0x93:
            if(!group(v,a,1))return false;
            break;
        case 0x94: {
            uint8_t *target=actor(v,arg);
            if(!target)return fail(v,1,op);
            if(!group(v,target,0))return false;
            break;
        }
        case 0x95: {
            uint8_t *target=actor(v,arg);
            if(!target)return fail(v,1,op);
            memset(target,0,ACTOR_BYTES);
            /* SUB FB5E increments the cleared actor's index, then reads its
             * zero-page delay before the outer loop observes deletion. Keep
             * that dead index; a deleted actor has no sequence to resolve. */
            if(target==a){next(a);return true;}
            break;
        }
        case 0x96: {
            uint8_t *target=actor(v,arg);
            if(!target)return fail(v,1,op);
            target[0x25]=3;break;
        }
        case 0x98: a[0x4e]|=2;break;
        case 0x99: case 0x9a:
            a[0x20]=arg;a[0x21]=c[2];a[0x22]=c[3];a[0x23]=0;
            a[0x25]|=4;break;
        case 0x9b: case 0x9c:
            a[0x4e]|=op==0x9b?1:128;
            if(v->effects && v->effects->command &&
               !v->effects->command(v->effects->context,v->id,op,arg,value,longword(a+0x32)))return fail(v,2,op);
            break;
        case 0xa0: case 0xa1: put_word(a+4,value);break;
        case 0xa7: a[0x16]=arg;break;
        case 0xa9:
            a[0x4f]=0;
            if(!(v->clock->pcm_flags&2)) { a[0x4f]=1;return true; }
            break;
        case 0xac: a[0x26]=1;break;
        case 0xaf: a[0x1c]=arg;break;
        default: return fail(v,2,op);
        }
        next(a);
    }
    return fail(v,4,0);
}

bool resource_actor_tick_registers(uint8_t actors[64*80], const void *resource,
    size_t size, uint32_t base, resource_actor_clock *clock, uint64_t mask,resource_actor_effects *effects,uint16_t *iteration_register) {
    if(!actors || !resource || !clock || !size)return false;
    vm v={actors,resource,size,base,clock,effects,0};
    clock->commands=0;clock->error=0;clock->error_actor=65535;
    clock->error_command=0;
    for(unsigned id=0;id<ACTORS;id++) {
        if(iteration_register)*iteration_register=clock->iterations;
        if(!(mask&(UINT64_C(1)<<id)))continue;
        v.id=id;uint8_t *a=actor(&v,id);
        if((a[0x1c] || a[0x26]) && clock->freeze)continue;
        if(word(a)!=1)continue;
        if(a[0x25]&4) {
            if(clock->pcm_clock<longword(a+0x20))continue;
            a[0x25]&=~4;
        }
        if(a[0x25]&1)continue;
        unsigned iterations=clock->iterations;
        do {
            if(!step(&v,a))return false;
            unsigned before=iterations;iterations=(uint16_t)(iterations-1);
            if(iteration_register)*iteration_register=iterations;
            if(!before || !iterations || !a[0x26])break;
        } while(word(a)==1);
    }
    /* SUBCODE F850 resolves the current sprite map after every actor stepped.
     * A control instruction retains the previous map. */
    for(unsigned id=0;id<ACTORS;id++) {
        if(!(mask&(UINT64_C(1)<<id)))continue;
        v.id=id;uint8_t *a=actor(&v,id);
        if(word(a)!=1 || (a[0x1c]&clock->freeze))continue;
        unsigned index=word(a+0x28);
        if(index==65535)index=0;
        int32_t displacement=(int16_t)(index*4);
        const uint8_t *c=address(&v,longword(a+0x32)+displacement,4);
        if(!c)return fail(&v,1,0);
        if(c[0]<128) {
            uint32_t maps=longword(a+0x2e);
            const uint8_t *entry=address(&v,maps+c[0]*2,2);
            if(!entry || !address(&v,maps+word(entry),2))return fail(&v,1,c[0]);
            put_long(a+0x36,maps+word(entry));
        }
        if(!(a[0x25]&1)) {
            put_long(a+6,longword(a+6)+longword(a+0x40));
            put_long(a+10,longword(a+10)+longword(a+0x44));
        }
    }
    return true;
}
bool resource_actor_tick_effects(uint8_t actors[64*80],const void *resource,size_t size,
    uint32_t base,resource_actor_clock *clock,uint64_t mask,resource_actor_effects *effects){
    return resource_actor_tick_registers(actors,resource,size,base,clock,mask,effects,NULL);
}
bool resource_actor_tick(uint8_t actors[64*80],const void *resource,size_t size,
    uint32_t base,resource_actor_clock *clock,uint64_t mask){
    return resource_actor_tick_effects(actors,resource,size,base,clock,mask,NULL);
}
