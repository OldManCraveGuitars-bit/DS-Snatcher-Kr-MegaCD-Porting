#include "system_perf.h"
#include "native_entry.h"
#include "resource_entry.h"
#include "resource_archive.h"
#include "resource_frame.h"
#include "resource_picture_row.h"
#include "resource_vdp_history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#ifdef __NDS__
#include <nds.h>
#endif
#if defined(__NDS__) && defined(SNATCHER_ENTRY_PROFILE)
#include <nds.h>
volatile unsigned entry_render_cycles,entry_snapshot_cycles,entry_fill_calls,entry_fill_frames,entry_source_frame;
volatile unsigned entry_planes_cycles,entry_sprites_cycles,entry_maps_cycles,entry_compose_cycles;
extern uint32_t resource_background_profile[6];
#define PROFILE_START(name) unsigned name=cpuGetTiming()
#define PROFILE_ADD(name,start) name+=cpuGetTiming()-start
#else
#define PROFILE_START(name)
#define PROFILE_ADD(name,start)
#endif
#define MAX_HISTORY ((NATIVE_RETURN_BUFFER_FRAMES*60+31999)/32000+2)
static unsigned history_count;
/* All playback retains every source snapshot using immutable shared rows.
 * First entry no longer reserves eighteen duplicate full pictures.
 * Vehicle playback retains the verified 12288-sample/26-picture reserve.
 * First entry uses the same reserve with shared rows for taller pictures.
 * Every source VBlank is still saved and presented. */
typedef struct {volatile uint32_t sample;uint16_t palette[64];uint16_t source_height,reserved;uint32_t voice,voice_begin,voice_end;uint8_t **rows;resource_vdp_packet *vdp;uint32_t sequence;uint16_t source_y;uint8_t pixels[];} frame;
static uint8_t *render_pixels;
static unsigned history_height,pixel_serial;
static bool last_snapshot_solid;
static resource_background_picture staging_picture;
/* Reconstruction runs below load_checkpoint's saved story state. Keep its
 * PCM scratch off the6KiB ARM9 stack, shared by scene decoding and IRQs. */
static int16_t warmup_audio[1070];
static resource_entry_state *engine;
static uint8_t *script,*arrival_script,*subcode,*table,*resident,*resources[3];
static size_t arrival_bytes;
static size_t resource_bytes[3];
static unsigned written,latest,used;
static frame **history;
typedef struct picture_row {struct picture_row *next,*hash_next;uint16_t refs,bytes;unsigned hash,serial;uint8_t pixels[];} picture_row;
/* Stable while a row is shared; recycled allocations always receive a new
 * identity. On counter exhaustion disable reuse, never alias an old row. */
static unsigned row_serial;
static void *row_blocks[MAX_HISTORY];
static picture_row *free_rows;
static bool flight,arriving,borrowed_audio_inputs,returning;
static uint8_t *frame_view_pixels;
volatile unsigned native_rows_copied,native_rows_shared,native_rows_peak;
static unsigned rows_in_use;
volatile unsigned native_row_allocation_failures,native_rows_retired;
#define ROW_BUCKETS 1024
static picture_row *row_buckets[ROW_BUCKETS];
static uint32_t solid_row[48];
volatile unsigned native_rows_interned;
volatile unsigned native_rows_live_bytes,native_rows_peak_bytes;
static resource_vdp_encoder *vdp_encoder;
static resource_vdp_view *vdp_view;
static unsigned vdp_produced,vdp_height,vdp_y,vdp_row_stamp,vdp_row_serials[192];
static volatile unsigned vdp_applied,vdp_shown;
volatile unsigned native_vdp_live_bytes,native_vdp_peak_bytes,native_vdp_packets,native_vdp_tiles,native_vdp_draws;
static void release_vdp(void){
    free(vdp_encoder);vdp_encoder=NULL;free(vdp_view);vdp_view=NULL;
    vdp_produced=vdp_applied=vdp_shown=vdp_height=vdp_y=vdp_row_stamp=0;
    memset(vdp_row_serials,0,sizeof(vdp_row_serials));
}
static unsigned row_hash(const uint8_t *pixels){
    const uint32_t *p=(const uint32_t*)pixels;unsigned h=2166136261u;
    for(unsigned x=0;x<48;x++)h=(h^p[x])*16777619u;
    return h;
}
static void unindex_row(picture_row *r){
    picture_row **link=row_buckets+(r->hash&(ROW_BUCKETS-1));
    while(*link && *link!=r)link=&(*link)->hash_next;
    if(*link)*link=r->hash_next;
    r->hash_next=NULL;
}
static picture_row *row_header(uint8_t *pixels){
    return (picture_row *)(pixels - offsetof(picture_row,pixels));
}
static void release_frame_rows(frame *f){
    if(!f)return;
    if(f->vdp){native_vdp_live_bytes-=f->vdp->bytes;free(f->vdp);f->vdp=NULL;}
    if(!f->rows)return;
    for(unsigned y=0;y<f->source_height;y++)if(f->rows[y]){
        picture_row *r=row_header(f->rows[y]);
        if(!--r->refs){
            unindex_row(r);rows_in_use--;
            if(r->bytes){native_rows_live_bytes-=offsetof(picture_row,pixels)+r->bytes;free(r);}
            else{r->next=free_rows;free_rows=r;}
        }
        f->rows[y]=NULL;
    }
    f->source_height=0;
}
static void release_row_pool(void){
    for(unsigned i=0;i<MAX_HISTORY;i++){free(row_blocks[i]);row_blocks[i]=NULL;}
    memset(row_buckets,0,sizeof(row_buckets));
    free_rows=NULL;rows_in_use=0;free(frame_view_pixels);frame_view_pixels=NULL;
}
static bool grow_row_pool(void){
    for(unsigned i=0;i<history_count;i++){
        if(row_blocks[i])continue;
        size_t stride=offsetof(picture_row,pixels)+192;
        row_blocks[i]=calloc(history_height,stride);
        if(!row_blocks[i]){native_row_allocation_failures++;return false;}
        for(unsigned y=0;y<history_height;y++){
            picture_row *r=(picture_row*)((uint8_t*)row_blocks[i]+y*stride);r->next=free_rows;free_rows=r;
        }
        return true;
    }
    return false;
}
static bool allocate_row_pool(void){
    native_rows_live_bytes=native_rows_peak_bytes=0;
    memset(solid_row,128,sizeof(solid_row));return returning || !flight || grow_row_pool();
}
static bool store_rows(frame *f,const frame *previous,unsigned height,bool solid,bool was_solid){
    f->source_height=height;
    for(unsigned y=0;y<height;y++){
        bool share=previous && previous->source_height==height &&
            ((solid && was_solid) || (!solid && !was_solid && !resource_background_last_row_changed(y)));
        if(share){
            f->rows[y]=previous->rows[y];row_header(f->rows[y])->refs++;native_rows_shared++;
        }else{
            const uint8_t *pixels=solid?(const uint8_t*)solid_row:render_pixels+y*192;
            uint8_t encoded[RESOURCE_PICTURE_ROW_MAX];
            /* Initial city scroll also retains26 native pictures. Use the
             * lossless shared-row representation instead of large raw slabs;
             * foreground presentation already expands this exact row codec. */
            unsigned bytes=(returning || !flight)?resource_picture_row_pack(pixels,encoded):0;
            const uint8_t *stored=bytes?encoded:pixels;unsigned length=bytes?bytes:192;
            unsigned hash=row_hash(pixels),bucket=hash&(ROW_BUCKETS-1);
            picture_row *r=row_buckets[bucket];
            while(r && (r->hash!=hash || r->bytes!=bytes || memcmp(r->pixels,stored,length)))r=r->hash_next;
            if(r){f->rows[y]=r->pixels;r->refs++;native_rows_interned++;continue;}
            if(bytes){
                r=malloc(offsetof(picture_row,pixels)+bytes);
                if(!r){native_row_allocation_failures++;return false;}
                native_rows_live_bytes+=offsetof(picture_row,pixels)+bytes;
                if(native_rows_live_bytes>native_rows_peak_bytes)native_rows_peak_bytes=native_rows_live_bytes;
            }else{
                if(!free_rows && !grow_row_pool())return false;
                r=free_rows;if(!r)return false;free_rows=r->next;
            }
            r->next=NULL;r->refs=1;r->bytes=bytes;r->hash=hash;
            r->serial=row_serial==UINT32_MAX?0:++row_serial;
            memcpy(r->pixels,stored,length);
            r->hash_next=row_buckets[bucket];row_buckets[bucket]=r;
            /* Pixel bytes stay immutable until the final owner releases
             * them. Publish once; repeated VBlanks need no cache writeback. */
#ifdef __NDS__
            if(!bytes)DC_FlushRange(r->pixels,192);
#endif
            f->rows[y]=r->pixels;native_rows_copied++;
            rows_in_use++;if(rows_in_use>native_rows_peak)native_rows_peak=rows_in_use;
        }
    }
    return true;
}
const uint8_t *native_entry_row_pixels(const uint8_t *row,uint8_t scratch[192]){
    if(!row)return NULL;
    picture_row *r=row_header((uint8_t*)row);
    return !r->bytes?row:resource_picture_row_unpack(row,r->bytes,scratch)?scratch:NULL;
}
unsigned native_entry_row_serial(const uint8_t *row){
    return row?row_header((uint8_t*)row)->serial:0;
}

static resource_portrait_history portrait_history;
static resource_portrait_snapshot portrait_frames[MAX_HISTORY];
static FILE *archive,*wave;
static unsigned wave_id;
static unsigned external_voice,prepared_voice;
static int state;
static uint32_t (*random_seed)(void);
static void (*random_retire)(uint32_t);
void native_entry_set_random_io(uint32_t (*seed)(void),void (*retire)(uint32_t)){
    random_seed=seed;random_retire=retire;
}
bool native_entry_random_state(uint32_t *value,bool store){
    if(!engine || !engine->video.scene.subcode || !value)return false;
    uint8_t *p=engine->video.scene.globals+10;
    if(store){for(unsigned i=0;i<4;i++)p[i]=*value>>(24-i*8);}
    else{*value=0;for(unsigned i=0;i<4;i++)*value=(*value<<8)|p[i];}
    return true;
}
static bool (*music_backend)(unsigned command);
static unsigned origin,voice_end,handoff_sample;
volatile unsigned native_entry_diagnostic[8];
static bool request_music(void *context,unsigned command){
    (void)context;return music_backend && music_backend(command);
}
static unsigned word(const uint8_t *p){return (unsigned)p[0]*256+p[1];}
static const uint8_t *load_file(void *context,unsigned id,size_t *bytes){
    (void)context;
    unsigned index=returning?(id==25?0:id==16?1:3):flight?(id==32?0:id==25?1:3):id==22?0:id==9?1:id==8?2:3;
    if(index==3)return NULL;
    if(returning && id==16 && !resources[1]){
        /* The original file command replaces K1 with H08. Past pictures
         * own their pixel bytes and never reference this script bank. */
        free(resources[0]);resources[0]=NULL;resource_bytes[0]=0;
        resources[1]=resource_archive_load(archive,"DATA_H08.BIN",resource_bytes+1);
    }
    *bytes=resource_bytes[index];return resources[index];
}
static bool load_pcm(void *context,unsigned sector,unsigned count,void *destination){
    (void)context;
    for(unsigned k=0;k<(returning?12u:flight?11u:2u);k++){
        unsigned id=returning?(k?169+k:67):flight?(k==10?16:k==9?37:k>=2?128+k:k?2:129):k?55:34;
        unsigned first=word(table+id*8+4)+0x17,n=word(table+id*8+6);
        if(sector<first || sector-first>=n || count>n-(sector-first))continue;
        if(!wave || wave_id!=id){
            if(wave)fclose(wave);
            char path[64];
            if(returning)snprintf(path,sizeof(path),"nitro:/native-speech-%04u-r22.bin",id);
            else if(flight && id==16)snprintf(path,sizeof(path),"%s","nitro:/native-pcm-bank-0016-r22.bin");
            else if(flight && k>=2)snprintf(path,sizeof(path),"nitro:/native-speech-%04u-r22.bin",id);
            else snprintf(path,sizeof(path),"%s",flight?(k?"nitro:/native-forecourt-pcm2-r22.bin":"nitro:/native-flight-voice129-r22.bin"):k?"nitro:/native-entry-voice55-r22.bin":"nitro:/native-entry-pcm-bank-r22.bin");
            wave=fopen(path,"rb");wave_id=id;
        }
        return wave && !fseek(wave,(sector-first)*2048,SEEK_SET) && system_perf_fread(destination,2048,count,wave)==count;
    }
    return false;
}
static bool return_transferred,initial_transferred;
void native_entry_end(void){
    uint32_t retired;
    if(random_retire && native_entry_random_state(&retired,false))random_retire(retired);
    return_transferred=initial_transferred=false;
    if(engine)resource_background_bind(NULL,0);
    if(archive)fclose(archive);
    if(wave)fclose(wave);
    archive=wave=NULL;resource_entry_end(engine);free(engine);engine=NULL;
    free(render_pixels);render_pixels=NULL;
    free(script);free(arrival_script);free(subcode);
    if(!borrowed_audio_inputs){free(table);free(resident);}borrowed_audio_inputs=false;
    if(history){for(unsigned i=0;i<history_count;i++){resource_portrait_snapshot_release(portrait_frames+i);release_frame_rows(history[i]);free(history[i]);}free(history);}
    release_row_pool();
    release_vdp();
    for(unsigned i=0;i<3;i++){free(resources[i]);resources[i]=NULL;resource_bytes[i]=0;}
    script=arrival_script=subcode=table=resident=NULL;history=NULL;arrival_bytes=0;
    written=used=latest=wave_id=origin=voice_end=handoff_sample=external_voice=prepared_voice=0;state=0;flight=arriving=returning=false;
    memset(&portrait_history,0,sizeof(portrait_history));
    history_height=0;pixel_serial=0;last_snapshot_solid=false;resource_background_picture_invalidate(&staging_picture);
}
static uint8_t *read_script(const char *path,size_t *bytes){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    uint8_t *p=NULL;
    if(!fseek(f,0,SEEK_END)){
        long n=ftell(f);
        if(n>0 && n<=65536 && !fseek(f,0,SEEK_SET)){
            p=malloc(n);
            if(p && system_perf_fread(p,1,n,f)!=(size_t)n){free(p);p=NULL;}
            if(p)*bytes=n;
        }
    }
    fclose(f);return p;
}
static bool begin(unsigned vehicle,bool (*music)(unsigned command),const resource_entry_audio *audio,unsigned entry_pc){
    memset((void *)native_entry_diagnostic,0,sizeof(native_entry_diagnostic));native_entry_diagnostic[0]=1;
    uint32_t source_seed=random_seed?random_seed():0;
    native_entry_end();flight=vehicle;
    history_count=((vehicle?NATIVE_FLIGHT_BUFFER_FRAMES:NATIVE_ENTRY_BUFFER_FRAMES)*60+31999)/32000+2;
    archive=fopen(flight?"nitro:/native-flight-banks-r22.nrs":"nitro:/native-resource-banks-r22.nrs","rb");
    music_backend=music;
    if(!archive)return false;
    size_t a=0,b=0,c=0,d=0;
    script=read_script(flight?"nitro:/native-flight-script-r22.bin":"nitro:/native-entry-script-r22.bin",&a);
    if(flight)arrival_script=read_script("nitro:/native-arrival-script-r22.bin",&arrival_bytes);
    subcode=resource_archive_load(archive,"SUBCODE.BIN",&b);
    if(audio){
        borrowed_audio_inputs=true;
        table=(uint8_t*)audio->pcm->bank.table;c=audio->pcm->bank.table_bytes;
        resident=(uint8_t*)audio->pcm->bank.resident;d=audio->pcm->bank.resident_bytes;
    }else{table=resource_archive_load(archive,"PCMLT_01.BIN",&c);resident=resource_archive_load(archive,"PCMDRMDT.BIN",&d);}
    /* These three original banks total only67KiB. Stage them before the
     * stream starts so a scene change cannot block the audio callback. */
    const char *names[]={"DATA_H15.BIN","DATA_H01.BIN","DATA_H00.BIN"};
    for(unsigned i=0;i<(flight?2u:3u);i++){
        resources[i]=resource_archive_load(archive,flight?(i?"DATA_K1.BIN":"DATA_S0.BIN"):names[i],resource_bytes+i);
        if(!resources[i]){native_entry_end();return false;}
    }
    native_entry_diagnostic[0]=2;
    engine=calloc(1,sizeof(resource_entry_room_storage));history=calloc(history_count,sizeof(*history));
    render_pixels=calloc(192*(flight?128:160),1);
    history_height=flight?128:160;
    unsigned frame_bytes=sizeof(frame)+sizeof(uint8_t*)*history_height;
    if(!flight && !allocate_row_pool()){native_entry_end();return false;}
    /* Keep all26 source snapshots. A long fresh game fragments the ordinary
     * DS heap; requiring one940KiB block failed before the first departure.
     * Store each original row once. The display repeats rows by DMA without
     * a pixel decoder inside VBlank, leaving time for uninterrupted audio. */
    if(history)for(unsigned i=0;i<history_count;i++){
        history[i]=calloc(1,frame_bytes);
        if(history[i] && !flight)history[i]->rows=(uint8_t**)(history[i]+1);
        if(!history[i]){native_entry_diagnostic[7]=i;native_entry_end();return false;}
    }
    native_entry_diagnostic[1]=sizeof(*engine);native_entry_diagnostic[2]=history_count*frame_bytes;
    native_entry_diagnostic[3]=engine!=NULL;native_entry_diagnostic[4]=history!=NULL;
    resource_pcm_effect_bank pcm={table,resident,NULL,c,d,0};resource_entry_io io={NULL,load_file,load_pcm,request_music};
    if(!script || (flight && !arrival_script) || (!render_pixels) || !subcode || !table || !resident || !engine || !history ||
       !resource_entry_begin_at_audio(engine,vehicle==2?arrival_script:script,vehicle==2?arrival_bytes:a,subcode,b,&pcm,&io,source_seed,vehicle==2?25:flight?32:22,vehicle==2?0x72:flight?0x1cf5:entry_pc,audio)){native_entry_end();return false;}
    native_entry_diagnostic[0]=3;return true;
}
static bool begin_vehicle_vdp(void){
    vdp_encoder=calloc(1,sizeof(*vdp_encoder));vdp_view=calloc(1,sizeof(*vdp_view));
    native_vdp_live_bytes=native_vdp_peak_bytes=native_vdp_packets=native_vdp_tiles=native_vdp_draws=0;
    if(!vdp_encoder || !vdp_view ||
       !resource_vdp_history_begin(vdp_encoder,vdp_view,&engine->video) ||
       !resource_vdp_history_bind(vdp_view))return false;
    engine->video.deferred_display=true;return true;
}
bool native_entry_begin(bool (*music)(unsigned command)){return begin(false,music,NULL,0);}
bool native_entry_begin_audio(bool (*music)(unsigned),const resource_entry_audio *a){return a && begin(false,music,a,0);}
bool native_entry_begin_title_audio(bool (*music)(unsigned),const resource_entry_audio *a){return a && begin(false,music,a,0x26);}
bool native_entry_flight_begin(bool (*music)(unsigned command),const resource_entry_cd *cd){
    return native_entry_flight_begin_audio(music,cd,NULL);
}
bool native_entry_flight_begin_audio(bool (*music)(unsigned command),const resource_entry_cd *cd,const resource_entry_audio *audio){
    if(!cd || !begin(true,music,audio,0))return false;
    resource_entry_set_cd(engine,cd);
    /* Recreate the genuine cockpit load to its first dialogue boundary.
     * Its previous user-paced dialogue has already been shown by StoryVM. */
    int result=0;
    for(unsigned i=0;i<120 && !result;i++){
        unsigned end=((uint64_t)(engine->video.frames+1)*32000*896040+53693174)/53693175;
        unsigned n=end-engine->samples;
        if(n>535){native_entry_end();return false;}
        result=resource_entry_render(engine,warmup_audio,n);
    }
    if(result!=1 || engine->handoff!=0x1cfe || !resource_entry_continue(engine,0x1f45)){
        native_entry_diagnostic[0]=4;native_entry_diagnostic[5]=engine->error;
        native_entry_diagnostic[6]=engine->handoff;native_entry_diagnostic[7]=result;
        native_entry_end();return false;
    }
    origin=engine->samples;if(!begin_vehicle_vdp()){native_entry_end();return false;}native_entry_diagnostic[0]=5;return true;
}
bool native_entry_forecourt_continue(void){
    if(!engine || !flight || state!=1 || engine->handoff!=0x18 ||
       !resource_entry_continue(engine,0x72))return false;
#if defined(__NDS__) && defined(SNATCHER_ENTRY_PROFILE)
    cpuStartTiming(0);
#endif
    state=0;handoff_sample=0;return true;
}
static bool snapshot(unsigned sample){
    unsigned slot=used?(latest+1)%history_count:0;
    frame *f=history[slot];
    if(vdp_view && f->sequence>vdp_applied)return false;
    f->sample=UINT32_MAX;release_frame_rows(f);
    bool compact=engine->file==8 || engine->file==32 || engine->file==25 || engine->file==16;
    unsigned height=compact?128:160;
    if(height>history_height)return false;
    if(vdp_view){
        if(!vdp_encoder || !vdp_view || vdp_produced==UINT32_MAX ||
           !resource_vdp_history_capture(vdp_encoder,&engine->video,&f->vdp))return false;
        f->sequence=++vdp_produced;f->source_y=engine->file==9?28:compact?8:0;
        if(f->vdp){
            native_vdp_live_bytes+=f->vdp->bytes;native_vdp_packets++;native_vdp_tiles+=f->vdp->tiles;
            if(native_vdp_live_bytes>native_vdp_peak_bytes)native_vdp_peak_bytes=native_vdp_live_bytes;
        }
    }else{
    bool solid=true;
    for(unsigned i=1;i<64;i++)if(engine->presentation.colors[i]!=engine->presentation.colors[0]){solid=false;break;}
    /* Equal black RGB values no longer imply equal pixels: source backdrop
     * is transparent while original black pattern pixels remain opaque. */
    if(resource_background_has_ui_backdrop() && !(engine->presentation.colors[0]&32767))solid=false;
    f->reserved=0x8000|((pixel_serial+1)&0x7fff);
    if(solid){
        if(used && last_snapshot_solid && history[latest]->source_height==height)f->reserved=history[latest]->reserved;
    }
    else{
        /* H01's original ACT1/SNATCH ink occupies native y73..142. Its
         * centre is108, so a160-row viewport beginning at28 centres the
         * unchanged title on DS. Only blank rows are replaced by blank. */
        unsigned source_y=engine->file==9?28:compact?8:0;
        {
            const uint8_t *previous=used && !last_snapshot_solid && history[latest]->source_height==height?render_pixels:NULL;
            if(!resource_opening_draw_native_packed_retained(&engine->video,render_pixels,source_y,height,previous,&staging_picture))return false;
            if(previous && !resource_background_last_indices_changed())f->reserved=history[latest]->reserved;

        }
    }
#if defined(__NDS__) && defined(SNATCHER_ENTRY_PROFILE)
    if(!solid){
        entry_planes_cycles+=resource_background_profile[0];
        entry_maps_cycles+=resource_background_profile[4];entry_compose_cycles+=resource_background_profile[5];
        entry_sprites_cycles+=resource_background_profile[1]+resource_background_profile[2]+resource_background_profile[3];
    }
#endif
    if(!store_rows(f,used?history[latest]:NULL,height,solid,last_snapshot_solid))return false;
    last_snapshot_solid=solid;pixel_serial=f->reserved&0x7fff;
    }
    memcpy(f->palette,engine->presentation.colors,sizeof(f->palette));
    /* DS index zero belongs exclusively to the area outside the picture.
     * Original black, flashes and fades always use opaque index128. */
    f->source_height=height;
    /* Sample ownership is the same as the upper picture. Future source
     * faces never leak into the lower screen while audio is filling ahead. */
    /* Only the SP06 flight currently presents lower source portraits.
     * Startup has no lower faces; after SP07 handoff StoryVM owns speech.
     * Keep every original actor tick/upper snapshot, but do not compose an
     * invisible lower picture while the player waits at the forecourt. */
    if(flight && (returning || !arriving || external_voice)){
        if(!resource_portrait_snapshot_take(&portrait_history,portrait_frames+slot,
            used?portrait_frames+latest:NULL,&engine->video,engine->file))return false;
    }else resource_portrait_snapshot_release(portrait_frames+slot);
    __asm__ volatile("" ::: "memory");
    f->voice=engine->voice_id;
    f->voice_begin=engine->voice_begin?engine->voice_begin-origin:UINT32_MAX;
    f->voice_end=engine->voice_end?engine->voice_end-origin:UINT32_MAX;
    f->sample=sample;latest=slot;if(used<history_count)used++;return true;
}
bool native_entry_forecourt_begin(bool (*music)(unsigned command),const resource_entry_cd *cd){
    return native_entry_forecourt_begin_audio(music,cd,NULL);
}
bool native_entry_forecourt_begin_audio(bool (*music)(unsigned command),const resource_entry_cd *cd,const resource_entry_audio *audio){
    if(!cd || !begin(2,music,audio,0))return false;
    resource_entry_set_cd(engine,cd);
    /* A checkpoint stores the original story state, not GPU/PCM RAM.
     * Re-execute the genuine scene prefix before exposing its first picture. */
    int result=0;
    for(unsigned i=0;i<300 && !result;i++){
        unsigned end=((uint64_t)(engine->video.frames+1)*32000*896040+53693174)/53693175;
        unsigned n=end-engine->samples;
        if(n>535){native_entry_end();return false;}
        result=resource_entry_render(engine,warmup_audio,n);
    }
    if(result!=1 || engine->handoff!=0x97){native_entry_end();return false;}
    origin=engine->samples;state=1;arriving=true;
    if(!begin_vehicle_vdp()){native_entry_end();return false;}
    if(!snapshot(0)){native_entry_end();return false;}
    return true;
}
unsigned native_entry_audio(int16_t *out,unsigned frames){
#if defined(__NDS__) && defined(SNATCHER_ENTRY_PROFILE)
    entry_fill_calls++;entry_fill_frames+=frames;
#endif
    unsigned total=frames;
    if(!engine || state<0 || state==2){memset(out,0,frames*4);return total;}
    if(state==1 && !flight){
        if(engine->audio.render){if(!resource_entry_audio_tail(engine,out,frames))state=-1;}
        else memset(out,0,frames*4);
        return total;
    }
    while(frames){
        /* Never render past a source VBlank without saving its picture.
         * Draw chooses history by the played DAC cursor, not fill-ahead. */
        unsigned before=engine->video.frames,before_samples=engine->samples;
        unsigned boundary=((uint64_t)(before+1)*32000*896040+53693174)/53693175;
        unsigned n=boundary-engine->samples;if(n>frames)n=frames;
        PROFILE_START(render_begin);
        bool idle=state==1;
        int result=idle?resource_entry_idle_render(engine,out,n):resource_entry_render(engine,out,n);
        if(!idle || result)state=result;
        PROFILE_ADD(entry_render_cycles,render_begin);
        PROFILE_START(snapshot_begin);
        if(engine->video.frames!=before && !snapshot(((uint64_t)before*32000*896040+53693174)/53693175-origin))state=-1;
        PROFILE_ADD(entry_snapshot_cycles,snapshot_begin);
#if defined(__NDS__) && defined(SNATCHER_ENTRY_PROFILE)
        entry_source_frame=engine->video.frames;
#endif
        written=engine->samples-origin;
        if(returning && engine->file==16 && engine->program.pc==0x1b1 && !engine->tasks.active && !engine->finished){
            if(!resource_entry_yield_menu(engine,0x1b1))state=-1;
            else state=1;
        }
        if(engine->voice_begin && !(resource_entry_audio_pcm(engine)->flags&2) && !voice_end)voice_end=written;
        if(state==1 && flight && ((!arriving && engine->handoff==0x1f89) || (returning && engine->handoff==0xe38))){
            /* SP07 starts in the same live S0 scene. Preserve actors, RNG,
             * palette pipeline and audio clock across the module boundary. */
            if(!resource_entry_switch_program(engine,arrival_script,arrival_bytes,returning?0x10:4)){
                state=-1;unsigned produced=engine->samples-before_samples;
                memset(out+produced*2,0,(frames-produced)*4);break;
            }
            arriving=true;state=0;
            unsigned produced=engine->samples-before_samples;
            out+=produced*2;frames-=produced;continue;
        }
        if(state==1 && flight){
            if(!handoff_sample)handoff_sample=written;
            /* The script yields to StoryVM; all original actors, scroll
             * bands and sound devices continue on the same sample clock. */
            unsigned produced=engine->samples-before_samples;
            out+=produced*2;frames-=produced;continue;
        }
        if(state){
            unsigned produced=engine->samples-before_samples;
            if(state==1 && engine->audio.render){
                if(!resource_entry_audio_tail(engine,out+produced*2,frames-produced))state=-1;
            }else memset(out+produced*2,0,(frames-produced)*4);
            break;
        }
        out+=n*2;frames-=n;
    }
    return total;
}
static frame *select_frame(unsigned samples){
    if(!engine || !used)return NULL;
    frame *selected=NULL;
    for(unsigned i=0;i<used;i++)if(history[i]->sample<=samples && (!selected || history[i]->sample>selected->sample))selected=history[i];
    if(!selected)state=-2; /* History exhausted: never show future frames. */
    return selected;
}
const resource_portrait_snapshot *native_entry_portraits(unsigned samples){
    frame *selected=select_frame(samples);
    if(selected)for(unsigned i=0;i<used;i++)if(selected==history[i])return portrait_frames+i;
    return NULL;
}
static bool present_vdp(frame *f){
    if(!vdp_view || f->sequence<vdp_applied)return false;
    if(f->sequence==vdp_shown)return true;
    while(vdp_applied<f->sequence){
        frame *next=history[vdp_applied%history_count];
        if(next->sample==UINT32_MAX || next->sequence!=vdp_applied+1 ||
           !resource_vdp_history_apply(vdp_view,next->vdp))return false;
        vdp_applied++;
    }
    bool solid=true;
    for(unsigned i=1;i<64;i++)if(f->palette[i]!=f->palette[0]){solid=false;break;}
    bool same_height=vdp_height==f->source_height;
    bool changed=true;
    if(solid){
        changed=!vdp_shown || !last_snapshot_solid || !same_height;
        if(changed)memset(render_pixels,128,f->source_height*192);
        resource_background_picture_invalidate(&staging_picture);
    }else{
        const uint8_t *previous=vdp_shown && !last_snapshot_solid && same_height?render_pixels:NULL;
        /* The packed indexed renderer and patch application allocate no
         * memory. They are exclusively owned by foreground presentation;
         * producer publication never touches their bank or cached planes. */
        /* Apply every original packet, including offscreen changes. The
         * last displayed viewport records all map/pattern/SAT dependencies;
         * only an intersecting change needs new indexed composition. Palette
         * changes remain independent and are always published by the LCD. */
        changed=!previous || resource_vdp_history_changed(vdp_view,f->source_y,f->source_height);
        if(changed){
            if(!resource_vdp_history_draw(vdp_view,render_pixels,f->source_y,f->source_height,previous,&staging_picture))return false;
            changed=!previous || resource_background_last_indices_changed();native_vdp_draws++;
        }
    }
    if(changed){
        pixel_serial=(pixel_serial+1)&0x7fff;
        for(unsigned y=0;y<f->source_height;y++)if(solid || !vdp_shown || last_snapshot_solid ||
            !same_height || resource_background_last_row_changed(y)){
            /* The LCD's two pages retain immutable identities, even though
             * this contiguous source raster is updated in place. */
            vdp_row_serials[y]=vdp_row_stamp==UINT32_MAX?0:++vdp_row_stamp;
        }
    }
    f->reserved=0x8000|pixel_serial;last_snapshot_solid=solid;vdp_height=f->source_height;vdp_y=f->source_y;vdp_shown=f->sequence;
    return true;
}
bool native_entry_frame(unsigned samples,const uint8_t **pixels,const uint16_t **palette,unsigned *sample){
    if(!pixels || !palette || !sample)return false;
    frame *selected=select_frame(samples);if(!selected)return false;
    if(vdp_view){if(!present_vdp(selected))return false;*pixels=render_pixels;}
    else if(selected->rows){
        /* Compatibility consumer only; the DS VBlank uses frame_view and
         * reads immutable rows directly, without expanding the picture. */
        if(!frame_view_pixels)frame_view_pixels=malloc(192*history_height);
        if(!frame_view_pixels)return false;
        for(unsigned y=0;y<selected->source_height;y++){
            uint8_t *target=frame_view_pixels+y*192;
            const uint8_t *p=native_entry_row_pixels(selected->rows[y],target);if(!p)return false;
            if(p!=target)memcpy(target,p,192);
        }
        *pixels=frame_view_pixels;
    }else *pixels=selected->pixels;
    *palette=selected->palette;*sample=selected->sample;
    return true;
}
bool native_entry_frame_view(unsigned samples,const uint8_t **pixels,const uint8_t *const **rows,
    const uint16_t **palette,unsigned *sample){
    if(!pixels || !rows || !palette || !sample)return false;
    frame *f=select_frame(samples);if(!f)return false;
    if(vdp_view){if(!present_vdp(f))return false;*pixels=render_pixels;*rows=NULL;}
    else{*pixels=f->rows?NULL:f->pixels;*rows=(const uint8_t *const *)f->rows;}
    *palette=f->palette;*sample=f->sample;return true;
}
unsigned native_entry_pixel_revision(unsigned sample){
    frame *f=select_frame(sample);return f?f->reserved:0;
}
unsigned native_entry_contiguous_row_serial(unsigned sample,unsigned row){
    (void)sample;
    return vdp_view && vdp_shown && row<vdp_height?vdp_row_serials[row]:0;
}
bool native_entry_draw(unsigned samples,uint16_t out[256*192]){
    frame *f=select_frame(samples);if(!out || !f)return false;
    if(vdp_view && !present_vdp(f))return false;
    const uint16_t *palette=f->palette;
    bool backdrop=resource_background_has_ui_backdrop();
    memset(out,0,256*192*sizeof(*out));
    uint8_t scratch[192];
    for(unsigned y=0;y<f->source_height;y++){
        const uint8_t *row=vdp_view?render_pixels+y*192:f->rows?native_entry_row_pixels(f->rows[y],scratch):f->pixels+y*192;
        if(!row)return false;
        for(unsigned x=0;x<192;x++){
            unsigned code=row[x]&63;uint16_t value=palette[code];
            out[y*256+x+32]=backdrop && !code && !(value&32767)?value&32767:value;
        }
    }
    return true;
}
int native_entry_status(void){return state;}
bool native_entry_release_played_history(void){
    if(!engine || flight || state!=1 || engine->file!=8 || engine->handoff!=0x5a)return false;
    if(history){for(unsigned i=0;i<history_count;i++){resource_portrait_snapshot_release(portrait_frames+i);release_frame_rows(history[i]);free(history[i]);}free(history);history=NULL;release_row_pool();}
    used=latest=0;return true;
}
bool native_entry_take_initial_room(resource_room **room,uint8_t **resource,uint8_t **sub){
    if(!room || !resource || !sub || *room || *resource || *sub ||
       initial_transferred || !engine || flight || state!=1 || history ||
       engine->file!=8 || engine->handoff!=0x5a || !engine->initialized ||
       !engine->finished || engine->error || engine->tasks.active ||
       engine->owned_pcm || !engine->audio.pcm || !resources[2] || !subcode)return false;
    /* First entry yields at the pending dialogue (tick_phase1). Preserve
     * exactly the VDP/presentation that the old copying handoff exported;
     * the room owns subsequent actor ticks. No I/O or audio callback may
     * run between this transfer and music_player_entry_release(). */
    _Static_assert(offsetof(resource_entry_state,presentation)==offsetof(resource_room,presentation),"presentation prefix");
    _Static_assert(offsetof(resource_entry_state,program)==offsetof(resource_room,tasks),"VDP prefix size");
    resource_entry_room_storage *storage=(resource_entry_room_storage*)engine;
    size_t prefix=offsetof(resource_room,tasks);
    memset((uint8_t*)storage+prefix,0,sizeof(*storage)-prefix);
    storage->room.scene=2;
    *room=&storage->room;*resource=resources[2];*sub=subcode;
    engine=NULL;resources[2]=NULL;subcode=NULL;initial_transferred=true;
    return true;
}
bool native_entry_initial_transferred(void){return initial_transferred;}
bool native_entry_copy_room(resource_room *r){
    if(!r || !engine || flight || state!=1 || engine->file!=8 || engine->handoff!=0x5a)return false;
    memset(r,0,sizeof(*r));r->video=engine->video;r->presentation=engine->presentation;
    r->scene=2;return true;
}
unsigned native_entry_picture_height(unsigned sample){
    for(unsigned i=0;i<used;i++)if(history[i]->sample==sample)return history[i]->source_height;
    return 0;
}
unsigned native_entry_music(void){return engine?engine->music_pending:0;}
unsigned native_entry_handoff(void){return engine?engine->handoff:0;}
unsigned native_entry_written(void){return written;}
unsigned native_entry_handoff_sample(void){return flight?handoff_sample:written;}
unsigned native_entry_current_sample(void){return engine?engine->samples-origin:0;}
unsigned native_entry_voice_begin(void){return engine && engine->voice_begin?engine->voice_begin-origin:0;}
unsigned native_entry_voice_end(void){return voice_end;}

bool native_entry_external_voice_ready(void){
    return engine && flight && state==1 && !engine->error &&
        (engine->handoff==0x18 || engine->handoff==0x97);
}
bool native_entry_prepare_voice(unsigned voice){
    if(!native_entry_external_voice_ready() || external_voice || engine->tasks.active ||
       (voice!=2 && voice!=16 && voice!=37 && (voice<130 || voice>136)) || (resource_entry_audio_pcm(engine)->flags&3))return false;
    char path[64];
    if(voice==16)snprintf(path,sizeof(path),"%s","nitro:/native-pcm-bank-0016-r22.bin");
    else if(voice==2)snprintf(path,sizeof(path),"%s","nitro:/native-forecourt-pcm2-r22.bin");
    else snprintf(path,sizeof(path),"nitro:/native-speech-%04u-r22.bin",voice);
    FILE *f=fopen(path,"rb");if(!f)return false;
    unsigned bytes=word(table+voice*8+6)*2048;
    if(fseek(f,0,SEEK_END) || ftell(f)!=(long)bytes || fseek(f,0,SEEK_SET)){fclose(f);return false;}
    if(wave)fclose(wave);
    wave=f;wave_id=voice;prepared_voice=voice;return true;
}
bool native_entry_start_voice(unsigned voice){
    if(voice!=prepared_voice || !resource_entry_pcm(engine,voice))return false;
    external_voice=voice;prepared_voice=0;return true;
}
int native_entry_story_voice(unsigned played,unsigned *samples,bool *audible){
    if(!samples || !audible || !external_voice || !engine || state<0 || state==2)return -1;
    *samples=0;*audible=false;
    if(engine->external_done!=UINT32_MAX && played>=engine->external_done-origin)return 0;
    if(engine->voice_id!=external_voice || !engine->voice_begin)return 1;
    unsigned begin=engine->voice_begin-origin;
    if(played<begin)return 1;
    *samples=(uint64_t)(played-begin)*16277/32000;
    if(engine->voice_end && played>=engine->voice_end-origin)return 1;
    *audible=true;return 1;
}
void native_entry_story_voice_release(void){external_voice=prepared_voice=0;}

bool native_entry_pcm_effects_blocked(bool blocked){
 return engine && state==1 && resource_entry_pcm_effects_blocked(engine,blocked);
}

volatile unsigned native_return_fault;
bool native_entry_return_begin(resource_room *room,uint8_t fields[128],const volatile unsigned *stream_sample,
    const resource_entry_audio *audio,const resource_entry_cd *cd){
    native_return_fault=1;
    if(!room || !stream_sample || !audio || !audio->pcm || !cd)return false;
    native_entry_end();flight=arriving=returning=true;borrowed_audio_inputs=true;
    table=(uint8_t*)audio->pcm->bank.table;resident=(uint8_t*)audio->pcm->bank.resident;
    archive=fopen("nitro:/native-resource-banks-r22.nrs","rb");if(!archive)return false;
    size_t bytes=0;
    script=read_script("nitro:/native-arrival-script-r22.bin",&bytes);
    arrival_script=read_script("nitro:/native-return-script-r22.bin",&arrival_bytes);
    /* Borrow for validation; ownership passes only after take_room succeeds.
     * Reuse the actual live K1/SUBCODE and VDP instead of duplicating them. */
    resources[0]=(uint8_t*)room->video.scene.resource;
    resource_bytes[0]=room->video.scene.resource_bytes;
    history_count=MAX_HISTORY;history_height=128;
    native_return_fault=3;
    resource_entry_io io={NULL,load_file,load_pcm,request_music};
    if(!script)native_return_fault=32;
    else if(!arrival_script)native_return_fault=33;
    else if(!room->video.scene.subcode)native_return_fault=34;
    else if(!resources[0])native_return_fault=35;
    else if(!resource_entry_take_room((resource_entry_room_storage*)room,script,bytes,&io,25,0x4ac,fields,audio))native_return_fault=37;
    else native_return_fault=0;
    if(native_return_fault){
        resources[0]=NULL;resource_bytes[0]=0;
        native_entry_end();return false;
    }
    engine=&((resource_entry_room_storage*)room)->entry;
    subcode=(uint8_t*)engine->video.scene.subcode;
    resource_entry_set_cd(engine,cd);origin=engine->samples-*stream_sample;written=*stream_sample;
    return true;
}
bool native_entry_return_ready(void){
    if(!returning || !engine || history)return false;
    native_return_fault=4;
    history=calloc(history_count,sizeof(*history));if(!history)return false;
    render_pixels=calloc(192*history_height,1);
    vdp_encoder=calloc(1,sizeof(*vdp_encoder));vdp_view=calloc(1,sizeof(*vdp_view));
    native_vdp_live_bytes=native_vdp_peak_bytes=native_vdp_packets=native_vdp_tiles=native_vdp_draws=0;
    if(!render_pixels || !vdp_encoder || !vdp_view ||
       !resource_vdp_history_begin(vdp_encoder,vdp_view,&engine->video) ||
       !resource_vdp_history_bind(vdp_view)){native_entry_end();return false;}
    engine->video.deferred_display=true;
    for(unsigned i=0;i<history_count;i++){
        history[i]=calloc(1,sizeof(frame));
        if(!history[i]){native_return_fault=100+i;native_entry_end();return false;}
    }
    native_return_fault=5;
    bool good=snapshot(0);if(good)native_return_fault=0;return good;
}
bool native_entry_return_release_history(void){
    if(!returning || !engine || state!=1 || engine->handoff!=0x1b1)return false;
    for(unsigned i=0;i<history_count;i++){
        resource_portrait_snapshot_release(portrait_frames+i);release_frame_rows(history[i]);free(history[i]);
    }
    free(history);history=NULL;release_row_pool();release_vdp();free(render_pixels);render_pixels=NULL;used=latest=0;return true;
}
unsigned native_entry_return_voice(unsigned played){
    frame *f=select_frame(played);
    return f && f->voice_begin!=UINT32_MAX && played>=f->voice_begin &&
        (f->voice_end==UINT32_MAX || played<f->voice_end)?f->voice:0;
}
bool native_entry_return_rebind(void){
    if(vdp_view)return resource_vdp_history_bind(vdp_view);
    return engine && resource_background_bind_vram(engine->video.front_vram,engine->video.front_palette,
        32,64,0xe000,engine->video.plane_b,0,0,256,224);
}
bool native_entry_return_room(resource_room *room){
    if(!returning || state!=1 || !engine || engine->handoff!=0x1b1 || !resource_entry_export_room(engine,room))return false;
    room->video.deferred_display=false;return true;
}
bool native_entry_take_return_room(resource_room **room,uint8_t **resource,uint8_t **sub){
    if(!room || !resource || !sub || *room || *resource || *sub ||
       !returning || return_transferred || !engine || state!=1 ||
       engine->handoff!=0x1b1 || history || render_pixels || vdp_view ||
       !resources[1] || !subcode)return false;
    resource_entry_room_storage *storage=(resource_entry_room_storage*)engine;
    if(!resource_entry_release_room(storage))return false;
    storage->room.video.deferred_display=false;
    *room=&storage->room;*resource=resources[1];*sub=subcode;
    engine=NULL;resources[1]=NULL;subcode=NULL;return_transferred=true;
    return true;
}
bool native_entry_return_transferred(void){return returning && return_transferred;}
bool native_entry_is_return(void){return returning;}
void native_entry_return_retire(unsigned played){
    if(!returning || !engine || !history || !used)return;
    frame *keep=select_frame(played);if(!keep)return;
    /* Called only outside the presentation IRQ. The played cursor is
     * monotonic, so earlier snapshots cannot be selected by a later IRQ.
     * Keep the selected picture and latest producer even if audio is late. */
    unsigned sample=keep->sample;
    for(unsigned i=0;i<used;i++)if(i!=latest && history[i]->sample<sample && history[i]->sequence<=vdp_applied){
        history[i]->sample=UINT32_MAX;release_frame_rows(history[i]);
        resource_portrait_snapshot_release(portrait_frames+i);native_rows_retired++;
    }
}
bool native_entry_return_reserve_room(void){
    /* Bound pending original tile/register packets, leaving headroom for a
     * new publication. The separate displayed64KiB VDP replaces raster-row
     * history; all56 source slots and both audio capacities are unchanged. */
    if(!returning)return true;
    if(native_vdp_live_bytes>=64*1024)return false;
    /* Audio reserve and visible packets share the same source clock.
     * Byte occupancy alone does not bound a run of tiny/unchanged packets.
     * Pause look-ahead before it can wrap onto an unconsumed source slot. */
    if(history && used){
        unsigned next=(latest+1)%history_count;
        if(history[next]->sequence>vdp_applied)return false;
    }
    return true;
}


bool native_entry_return_resources(uint8_t **resource,uint8_t **sub){
    if(!returning || !engine || state!=1 || engine->handoff!=0x1b1 || !resources[1] || !subcode)return false;
    *resource=resources[1];*sub=subcode;resources[1]=NULL;subcode=NULL;return true;
}
