#include "settings.h"
#include "resource_entry.h"
#include <string.h>
#include <stdlib.h>
resource_pcm_driver *resource_entry_audio_pcm(resource_entry_state *s){return s->audio.pcm?s->audio.pcm:s->owned_pcm;}
bool resource_entry_set_audio(resource_entry_state *s,const resource_entry_audio *a){
    if(!s || !s->initialized || s->samples || s->video.frames || s->audio.pcm ||
       !a || !a->pcm || !a->render || !a->command || !a->status)return false;
    free(s->owned_pcm);s->owned_pcm=NULL;s->audio=*a;s->program.pcm_attenuation=&a->pcm->attenuation;return true;
}
void resource_entry_end(resource_entry_state *s){
    if(!s)return;
    free(s->owned_pcm);s->owned_pcm=NULL;memset(&s->audio,0,sizeof(s->audio));s->initialized=false;
}
static void audio_event(resource_entry_state *s,unsigned kind,unsigned a,unsigned b){
    if(s->audio.event)s->audio.event(s->audio.context,kind,a,b);
}
static void pcm_status(resource_entry_state *s){
    s->devices.pcm_flags=resource_entry_audio_pcm(s)->flags;
    if((s->devices.pcm_flags&2) && !s->voice_begin)s->voice_begin=s->samples;
    if(s->voice_begin && !(s->devices.pcm_flags&2) && !s->voice_end)s->voice_end=s->samples;
}
bool resource_entry_audio_tail(resource_entry_state *s,int16_t *out,unsigned frames){
    return s && s->finished && s->audio.render && s->audio.render(s->audio.context,out,frames);
}
static unsigned word(const uint8_t *p){return (unsigned)p[0]*256+p[1];}
static uint32_t longword(const uint8_t *p){return (uint32_t)word(p)<<16|word(p+2);}
static void putword(uint8_t *p,unsigned n){p[0]=n>>8;p[1]=n;}
static void putlong(uint8_t *p,uint32_t n){putword(p,n>>16);putword(p+2,n);}
static uint8_t *g(resource_entry_state *s,unsigned at){return s->video.scene.globals+at-0x7860;}
static bool request(void *context,unsigned kind,uint32_t at,unsigned value){
    resource_entry_state *s=context;
    if(kind==RESOURCE_TASK_PCM_START){
        if(!resource_pcm_driver_load(resource_entry_audio_pcm(s),value))return false;
        audio_event(s,4,value,0);
        if(resource_entry_audio_pcm(s)->ram[0x11]==2 || resource_entry_audio_pcm(s)->ram[0x11]==0x82){
            s->voice_id=value;s->voice_begin=s->voice_end=0;
        }
        s->pcm_load=value;s->devices.pcm_flags=resource_entry_audio_pcm(s)->flags;return true;
    }
    if(kind==RESOURCE_TASK_CD_START)return s->cd.start && s->cd.start(s->cd.context,value);
    if(kind==RESOURCE_TASK_CD_PAUSE)return !s->cd.pause || s->cd.pause(s->cd.context);
    if(kind!=RESOURCE_TASK_FILE_START || at!=0x28000)return false;
    size_t bytes=0;const uint8_t *data=s->io.file(s->io.context,value,&bytes);
    if(!data || bytes<32)return false;
    /* Original file loading replaces the bank, not actor/global state. */
    s->video.scene.resource=data;s->video.scene.resource_bytes=bytes;s->file=value;return true;
}
static bool sound(void *context,uint32_t entry,unsigned value,unsigned channel,resource_scene_devices *d){
    resource_entry_state *s=context;
    if(entry==0x150ee){
        if(!resource_pcm_driver_effect(resource_entry_audio_pcm(s),value,channel))return false;
        audio_event(s,3,value,channel);
        d->pcm_flags=resource_entry_audio_pcm(s)->flags;return true;
    }
    if(entry!=0x150fa)return false;
    if(!value)return true; /* Native15EB4 zero command. */
    s->music_pending=value;
    if(s->audio.command){if(!s->audio.command(s->audio.context,value,channel))return false;}
    else if(!s->io.music || !s->io.music(s->io.context,value))return false;
    s->music_pending=0;return true;
}
bool resource_entry_begin(resource_entry_state *s,const void *script,size_t script_bytes,
    const void *sub,size_t subbytes,const resource_pcm_effect_bank *pcm,const resource_entry_io *io,uint32_t rng){
    return resource_entry_begin_at(s,script,script_bytes,sub,subbytes,pcm,io,rng,22,0);
}
void resource_entry_set_cd(resource_entry_state *s,const resource_entry_cd *cd){
    if(s){if(cd)s->cd=*cd;else memset(&s->cd,0,sizeof(s->cd));}
}
static bool external_task(resource_entry_state *s,const uint8_t *commands,unsigned bytes){
    if(!s || !s->initialized || !s->finished || s->error || s->tasks.active)return false;
    return resource_tasks_begin(&s->tasks,commands,bytes);
}
bool resource_entry_pcm(resource_entry_state *s,unsigned voice){
    if(!s || !s->initialized || !resource_entry_audio_pcm(s)->bank.table || !voice || voice>=0x4000/8 || voice*8+8>resource_entry_audio_pcm(s)->bank.table_bytes || (resource_entry_audio_pcm(s)->flags&3))return false;
    unsigned mode=resource_entry_audio_pcm(s)->bank.table[voice*8+1];
    if(mode!=1 && mode!=2 && mode!=0x82)return false;
    /* The dispatched SP07 warning PCM commands are immediately followed
     * by source46/task10, including the final effect-bank reload2. */
    const uint8_t commands[]={0,13,(uint8_t)(voice>>8),(uint8_t)voice,0,10,0,0};
    if(!external_task(s,commands,sizeof(commands)))return false;
    s->external_pending=true;s->external_done=UINT32_MAX;
    s->voice_id=voice;s->voice_begin=s->voice_end=0;return true;
}
bool resource_entry_control(resource_entry_state *s,unsigned actor,unsigned command){
    if(actor>=64 || command>5)return false;
    const uint8_t commands[]={0,9,0,(uint8_t)command,0,(uint8_t)actor,0,0};
    return external_task(s,commands,sizeof(commands));
}
bool resource_entry_continue(resource_entry_state *s,uint16_t pc){
    return s && resource_entry_switch_program(s,s->program.script,s->program.bytes,pc);
}
bool resource_entry_switch_program(resource_entry_state *s,const void *script,size_t bytes,uint16_t pc){
    if(!s || !s->initialized || !s->finished || s->error || s->tasks.active)return false;
    resource_program_state program;resource_tasks_state tasks=s->tasks;
    if(!resource_program_begin(&program,script,bytes,pc,s->program.scene_key,s->program.word79f0))return false;
    program.fields=s->program.fields;program.pcm_attenuation=&resource_entry_audio_pcm(s)->attenuation;
    if(resource_program_next(&program,&tasks)!=1)return false;
    s->program=program;s->tasks=tasks;
    /* Keep the live tick phase, including actors already updated while
     * StoryVM waited for a user selection at this dialogue boundary. */
    s->finished=false;s->handoff=0;return true;
}
bool resource_entry_begin_at(resource_entry_state *s,const void *script,size_t script_bytes,
    const void *sub,size_t subbytes,const resource_pcm_effect_bank *pcm,const resource_entry_io *io,uint32_t rng,unsigned file,uint16_t pc){
    return resource_entry_begin_at_audio(s,script,script_bytes,sub,subbytes,pcm,io,rng,file,pc,NULL);
}
bool resource_entry_begin_at_audio(resource_entry_state *s,const void *script,size_t script_bytes,
    const void *sub,size_t subbytes,const resource_pcm_effect_bank *pcm,const resource_entry_io *io,
    uint32_t rng,unsigned file,uint16_t pc,const resource_entry_audio *audio){
    if(!s || !io || !io->file || !io->pcm)return false;
    memset(s,0,sizeof(*s));s->io=*io;
    if(audio){
        if(!audio->pcm || !audio->render || !audio->command || !audio->status)return false;
        s->audio=*audio;
    }else{
        s->owned_pcm=malloc(sizeof(*s->owned_pcm));
        if(!s->owned_pcm || !resource_pcm_driver_bind(s->owned_pcm,pcm) || !resource_pcm_output_reset(&s->output,32000)){
            resource_entry_end(s);return false;
        }
    }
    size_t bytes=0;const uint8_t *resource=io->file(io->context,file,&bytes);
    if(!resource || !resource_opening_bind_scene(&s->video,resource,bytes,sub,subbytes,rng) ||
       !resource_program_begin(&s->program,script,script_bytes,pc,65535,0)){resource_entry_end(s);return false;}
    s->program.pcm_attenuation=&resource_entry_audio_pcm(s)->attenuation;
    s->devices.context=s;s->devices.sound=sound;s->file=file;
    /* Original S0 observations: SUB palette -> shared RAM -> Main CRAM
     * takes one more VBlank than SAT publication (e.g.55193..55207).
     * Other scene profiles retain their separately verified pipeline. */
    s->video.delayed_palette=file==32;
    /* Original shared actor/font initialization, from SUBCODE entries.
     * Actor0 is excluded from top-screen text by the native SAT builder. */
    const uint8_t initial[]={0,1,0,0,0xed,0x0e,0,3,0,0,0xec,0xb6,0,3,0,0,0xec,0xc4,0,0};
    if(!resource_tasks_begin(&s->tasks,initial,sizeof(initial))){resource_entry_end(s);return false;}
    resource_opening_presentation_reset(&s->presentation);s->initialized=true;return true;
}
static bool deliver(resource_entry_state *s){
    if(!resource_entry_audio_pcm(s)->ram[0x0f])return true;
    unsigned sector=longword(resource_entry_audio_pcm(s)->ram+0x2c),count=longword(resource_entry_audio_pcm(s)->ram+0x30);
    if(!count || count>52)return false;
    for(unsigned i=0;i<count;i++){
        if(!s->io.pcm(s->io.context,sector+i,1,s->transfer) ||
           !resource_pcm_driver_feed_load(resource_entry_audio_pcm(s),s->transfer,sizeof(s->transfer)))return false;
        audio_event(s,5,sector+i,1);
    }
    return true;
}
static int tick(resource_entry_state *s){
    resource_opening_state *v=&s->video;
    if(!s->tick_phase){
        if(!deliver(s)){s->error=1;return -1;}
        s->devices.pcm_flags=resource_entry_audio_pcm(s)->flags;s->devices.frame_clock=v->frames;
        if(s->audio.status)s->audio.status(s->audio.context,&s->devices.music,&s->devices.effect);
        if(s->cd.clock)putlong(g(s,0x787a),s->cd.clock(s->cd.context));
        if(!resource_opening_publish(v)){s->error=2;return -1;}
        uint32_t rng=longword(g(s,0x786a));resource_actor_random(&rng);putlong(g(s,0x786a),rng);
        putword(g(s,0x78b2),1);
        if(!s->tasks.active && !s->finished){
            int next=resource_program_next(&s->program,&s->tasks);
            if(next<0){s->error=3;return -1;}
            if(next==2){s->tick_phase=1;s->finished=true;s->handoff=s->program.pc;return 1;}
        }
        s->tick_phase=1;
    }
    resource_tasks_io task_io={s,request,s->cd.status?s->cd.status(s->cd.context):0,false};
    /* SUB D6EE..D71A: complete the native plane/tile transfer before
     * advancing work-queue commands, including voice-scene tile loads. */
    if(!*g(s,0x7887) && !*g(s,0x7886) && !resource_tasks_tick(&s->tasks,&v->scene,&s->devices,&task_io)){
        if(s->music_pending)return 2;
        s->error=4;return -1;
    }
    if(s->external_pending && !s->tasks.active){s->external_done=s->samples;s->external_pending=false;}
    if(!*g(s,0x7887) && !*g(s,0x7886) && !resource_scene_actors_tick_devices(&v->scene,&s->devices)){
        /* Actor sound must have a live synchronous device. Retrying a
         * partially stepped actor pass could replay earlier side effects. */
        s->error=5;return -1;
    }
    if(!resource_opening_finish_tick(v)){s->error=6;return -1;}
    if(!s->audio.render){
        if(!resource_pcm_driver_tick(resource_entry_audio_pcm(s))){s->error=7;return -1;}
        pcm_status(s);
    }
    /* Select presentation at actual file/scene load time, not at the
     * program's look-ahead scene_key (which can precede a load by seconds). */
    v->scene_id=s->file;
    if(!resource_opening_presentation_tick(&s->presentation,v)){s->error=8;return -1;}
    s->next_tick=((uint64_t)v->frames*32000*896040+53693174)/53693175;
    s->tick_phase=0;
    return 0;
}
static int render(resource_entry_state *s,int16_t *out,unsigned frames,bool idle){
    if(!s || !s->initialized || !out)return -1;
    if(s->error)return -1;
    if(s->finished && !idle){memset(out,0,frames*4);return 1;}
    while(frames){
        if(s->samples==s->next_tick){
            int status=tick(s);if(status){memset(out,0,frames*4);return status;}
        }
        unsigned n=s->next_tick-s->samples;if(n>frames)n=frames;
        if(s->audio.render){
            if(!s->audio.render(s->audio.context,out,n)){s->error=10;return -1;}
            pcm_status(s);
        }else resource_pcm_output_render_masked(&s->output,&resource_entry_audio_pcm(s)->chip,out,n,settings_pcm_mask(resource_entry_audio_pcm(s)->ram[0x11]==2 || resource_entry_audio_pcm(s)->ram[0x11]==0x82));
        if(s->cd.mix && !s->cd.mix(s->cd.context,out,n)){s->error=9;return -1;}
        s->samples+=n;out+=n*2;frames-=n;
    }
    return 0;
}
int resource_entry_render(resource_entry_state *s,int16_t *out,unsigned frames){return render(s,out,frames,false);}
int resource_entry_idle_render(resource_entry_state *s,int16_t *out,unsigned frames){
    if(!s || !s->finished)return -1;
    return render(s,out,frames,true);
}
bool resource_entry_draw(resource_entry_state *s,uint16_t out[256*192]){
    if(!s || !s->initialized || !s->presentation.initialized)return false;
    /* Counter starts at its final full-screen view, preserving the native
     * palette fade without the original hardware zoom adaptation. */
    unsigned y=s->file==9?28:(s->file==8 || s->file==32)?8:0,h=(s->file==8 || s->file==32)?128:160;
    return resource_opening_draw_area(&s->video,out,s->presentation.colors,32,y,192,h);
}

bool resource_entry_pcm_effects_blocked(resource_entry_state *s,bool blocked){
 const uint8_t commands[]={0,29,0,blocked?1:0,0,0};
 if(!external_task(s,commands,sizeof(commands)))return false;
 /* StoryVM owns this boundary; task29 completes synchronously, before
  * the immediately following source PCM request. No actor/audio tick is
  * inserted or removed. The original queue processor owns the flag. */
 const resource_tasks_io io={s,request,0,false};
 return resource_tasks_tick(&s->tasks,&s->video.scene,&s->devices,&io) && !s->tasks.active;
}

static bool adopt_room(resource_entry_state *s,const resource_room *r,bool take,
    const void *script,size_t bytes,const resource_entry_io *io,unsigned file,
    uint16_t pc,uint8_t fields[128],const resource_entry_audio *audio){
    if(!s || !r || r->error || r->tasks.active || r->queued || !fields ||
       !audio || !audio->pcm || !io || !io->file || !io->pcm)return false;
    size_t bank_bytes=0;const uint8_t *bank=io->file(io->context,file,&bank_bytes);
    if(!bank || bank_bytes!=r->video.scene.resource_bytes ||
       memcmp(bank,r->video.scene.resource,bank_bytes))return false;
    /* Validation happens before changing the destination or live room. */
    resource_program_state program;
    if(!resource_program_begin(&program,script,bytes,pc,(file<<8)|r->scene,0))return false;
    if(!audio->render || !audio->command || !audio->status)return false;
    /* The VDP/presentation prefix is identical in both owners. Capture the
     * small room tail before replacing it; taking ownership never allocates
     * or copies a second live VDP, and validation above leaves it untouched. */
    _Static_assert(offsetof(resource_entry_state,presentation)==offsetof(resource_room,presentation),"presentation prefix");
    _Static_assert(offsetof(resource_entry_state,program)==offsetof(resource_room,tasks),"VDP prefix size");
    unsigned scene=r->scene;resource_scene_devices devices=r->devices;
    if(take){
        size_t prefix=offsetof(resource_entry_state,program);
        memset((uint8_t*)s+prefix,0,sizeof(*s)-prefix);
    }else{
        memset(s,0,sizeof(*s));s->video=r->video;s->presentation=r->presentation;
    }
    s->io=*io;s->audio=*audio;s->program=program;
    s->video.scene.resource=bank;s->file=file;s->scene=scene;
    s->program.fields=fields;s->program.pcm_attenuation=&audio->pcm->attenuation;
    s->devices=devices;s->devices.context=s;s->devices.sound=sound;
    s->samples=s->next_tick=((uint64_t)s->video.frames*32000*896040+53693174)/53693175;
    s->initialized=true;
    return resource_background_bind_vram(s->video.front_vram,s->video.front_palette,32,64,
        0xe000,s->video.plane_b,0,0,256,224);
}
bool resource_entry_adopt_room(resource_entry_state *s,const resource_room *r,
    const void *script,size_t bytes,const resource_entry_io *io,unsigned file,
    uint16_t pc,uint8_t fields[128],const resource_entry_audio *audio){
    if((const void*)s==(const void*)r)return false;
    return adopt_room(s,r,false,script,bytes,io,file,pc,fields,audio);
}
bool resource_entry_take_room(resource_entry_room_storage *storage,
    const void *script,size_t bytes,const resource_entry_io *io,unsigned file,
    uint16_t pc,uint8_t fields[128],const resource_entry_audio *audio){
    if(!storage)return false;
    return adopt_room(&storage->entry,&storage->room,true,script,bytes,io,file,pc,fields,audio);
}
bool resource_entry_export_room(const resource_entry_state *s,resource_room *r){
    if(!s || !r || !s->initialized || s->error || !s->finished ||
       s->tasks.active || s->tick_phase || s->program.scene_key>>8!=s->file)return false;
    memset(r,0,sizeof(*r));r->video=s->video;r->presentation=s->presentation;
    r->scene=s->program.scene_key&255;r->devices=s->devices;
    /* The new room owner must attach its own synchronous device callback. */
    r->devices.context=NULL;r->devices.sound=NULL;
    return resource_background_bind_vram(r->video.front_vram,r->video.front_palette,32,64,
        0xe000,r->video.plane_b,0,0,256,224);
}
bool resource_entry_release_room(resource_entry_room_storage *storage){
    if(!storage)return false;
    resource_entry_state *s=&storage->entry;
    if(!s->initialized || s->error || !s->finished || s->tasks.active ||
       s->tick_phase || s->program.scene_key>>8!=s->file || s->owned_pcm ||
       !s->audio.pcm)return false;
    /* Binding validates the geometry before changing any live state and
     * allocates nothing. Both union members have the identical VDP prefix. */
    if(!resource_background_bind_vram(s->video.front_vram,s->video.front_palette,
        32,64,0xe000,s->video.plane_b,0,0,256,224))return false;
    _Static_assert(offsetof(resource_entry_state,presentation)==offsetof(resource_room,presentation),"presentation prefix");
    _Static_assert(offsetof(resource_entry_state,program)==offsetof(resource_room,tasks),"VDP prefix size");
    unsigned scene=s->program.scene_key&255;
    resource_scene_devices devices=s->devices;
    size_t prefix=offsetof(resource_room,tasks);
    memset((uint8_t*)storage+prefix,0,sizeof(*storage)-prefix);
    storage->room.scene=scene;storage->room.devices=devices;
    storage->room.devices.context=NULL;storage->room.devices.sound=NULL;
    return true;
}
bool resource_entry_yield_menu(resource_entry_state *s,uint16_t pc){
    /* Only an original registered-menu root may transfer back to StoryVM.
     * Unknown commands still fail in the native linear task interpreter. */
    if(!s || !s->initialized || s->error || s->finished || s->tasks.active ||
       s->tick_phase || s->program.pc!=pc || (size_t)pc+2>=s->program.bytes ||
       s->program.script[pc]!=0x30 || s->program.script[pc+1]!=0x26)return false;
    s->finished=true;s->handoff=pc;return true;
}
