#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#endif
#ifdef __NDS__
#include <fat.h>
#endif
#include "save_game.h"
#include "hq_scene.h"
#include "resource_checksum.h"
#include "../generated/office_branch.h"
#include "../generated/text.h"
#define PAYLOAD_BYTES (204+STORY_STATE_BYTES)
static void (*io_service)(void);
void save_set_io_service(void (*service)(void)){io_service=service;}
static void service_io(void){if(io_service)io_service();}
#define FILE_BYTES (20+PAYLOAD_BYTES)
#define NATIVE_LIMIT (256u*1024u)
static int (*native_capture)(uint8_t **output,size_t *bytes);
static bool (*native_restore)(const uint8_t *input,size_t bytes);
static unsigned loaded_bank,loaded_slot;
static uint32_t loaded_sequence;
void save_set_native_io(int (*capture)(uint8_t **output,size_t *bytes),bool (*restore)(const uint8_t *input,size_t bytes)){
    native_capture=capture;native_restore=restore;
}
static bool ready;
static unsigned current_slot=1;
static char directory[96];
static const uint8_t magic[8]={'S','N','D','S','R','9',0,0};
static uint32_t get32(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static unsigned native_header(const uint8_t p[12]){
    unsigned n=get32(p+8);return !memcmp(p,"NVRS",4) && get32(p+4)==1 && n && n<=NATIVE_LIMIT?n:0;
}
bool checkpoint_valid(const GameCheckpoint *s){
    if(s->mode==18 || s->mode==19){
        if(s->count || !hq_scene_valid(s->line) || !story_blob_valid(s->story))return false;
        int text=story_blob_text(s->story);
        if(s->mode==18)return text>=0 && s->page<story_page_count[text];
        return !s->page && (s->story[12] || s->story[13]);
    }
    if(s->mode>=15 && s->mode<=17){
        if(s->count || s->page)return false;
        return s->mode==15?s->line==1:s->mode==16?s->line<2:s->line==0;
    }
    if(s->mode<8 || s->mode>10 || s->count>OFFICE_QUEUE_CAPACITY)return false;
    for(unsigned i=0;i<s->count;i++)if(s->queue[i]>=OFFICE_TEXT_COUNT || s->speakers[i]>=SPEAKER_COUNT)return false;
    if(s->mode==10 && (!s->count || s->line>=s->count || s->page>=office_page_count[s->queue[s->line]]))return false;
    return true;
}
static void encode(uint8_t *p,const GameCheckpoint *s){
    memcpy(p,s->state,OFFICE_STATE_BYTES);p+=OFFICE_STATE_BYTES;
    for(unsigned i=0;i<OFFICE_QUEUE_CAPACITY;i++){*p++=(uint8_t)s->queue[i];*p++=(uint8_t)(s->queue[i]>>8);}
    for(unsigned i=0;i<OFFICE_QUEUE_CAPACITY;i++){*p++=(uint8_t)s->speakers[i];*p++=(uint8_t)(s->speakers[i]>>8);}
    unsigned values[4]={s->count,s->mode,s->line,s->page};
    for(unsigned i=0;i<4;i++){*p++=(uint8_t)values[i];*p++=(uint8_t)(values[i]>>8);}
    memcpy(p,s->story,STORY_STATE_BYTES);
}
static void decode(GameCheckpoint *s,const uint8_t *p){
    memset(s,0,sizeof(*s));
    memcpy(s->state,p,OFFICE_STATE_BYTES);p+=OFFICE_STATE_BYTES;
    for(unsigned i=0;i<OFFICE_QUEUE_CAPACITY;i++){s->queue[i]=p[0]|(p[1]<<8);p+=2;}
    for(unsigned i=0;i<OFFICE_QUEUE_CAPACITY;i++){s->speakers[i]=p[0]|(p[1]<<8);p+=2;}
    s->count=p[0]|(p[1]<<8);p+=2;s->mode=p[0]|(p[1]<<8);p+=2;
    s->line=p[0]|(p[1]<<8);p+=2;s->page=p[0]|(p[1]<<8);
}
static void path(char out[128],unsigned bank){snprintf(out,128,"%s/slot%u-%c.sav",directory,current_slot,bank?'b':'a');}
unsigned save_current_slot(void){return current_slot;}
bool save_select_slot(unsigned slot){if(slot<1 || slot>SAVE_SLOT_COUNT)return false;current_slot=slot;return true;}
void save_init(void){
    current_slot=1;
#ifdef __NDS__
    ready=fatInitDefault();snprintf(directory,sizeof(directory),"%ssnatcher_ds",fatGetDefaultDrive());
#else
    const char *p=getenv("SNATCHER_TEST_SAVE_DIR");ready=p!=NULL;
    snprintf(directory,sizeof(directory),"%s",p?p:"");
#endif
}
static int read_bank(unsigned bank,GameCheckpoint *s,uint32_t *seq){
    service_io();
    char name[128];path(name,bank);errno=0;FILE *f=fopen(name,"rb");
    if(!f)return errno==ENOENT?SAVE_MISSING:SAVE_IO_ERROR;
    service_io();
    /* The audio service runs the native renderer. Keep nested checkpoint
     * and file buffers off the DS's6KiB user stack while it does so. Save
     * operations are serialized and the callback cannot re-enter them. */
    static uint8_t b[FILE_BYTES];memset(b,0,sizeof(b));
    size_t n=fread(b,1,sizeof(b),f);
    unsigned version=get32(b+8);uint32_t checksum=get32(b+16);put32(b+16,0);
    int result=SAVE_OK;uint32_t crc=resource_crc_update(~0u,b+8,n>=8?n-8:0);
    if(n==sizeof(b) && version==4){
        uint8_t header[12];unsigned remaining=0;
        if(fread(header,1,sizeof(header),f)!=sizeof(header) || !(remaining=native_header(header)))result=SAVE_INVALID;
        else{
            crc=resource_crc_update(crc,header,sizeof(header));static uint8_t chunk[4096];
            while(remaining){
                service_io();unsigned count=remaining>sizeof(chunk)?sizeof(chunk):remaining;
                if(fread(chunk,1,count,f)!=count){result=SAVE_INVALID;break;}
                crc=resource_crc_update(crc,chunk,count);remaining-=count;
            }
        }
    }
    int tail=fgetc(f);bool error=ferror(f);int close=fclose(f);service_io();
    if(error || close)return SAVE_IO_ERROR;
    if(result || (n!=224 && n!=sizeof(b)) || tail!=EOF)return SAVE_INVALID;
    if(memcmp(b,magic,8) || version<1 || version>4 || n!=(version>=3?sizeof(b):224) || !get32(b+12) || checksum!=~crc)return SAVE_INVALID;
    decode(s,b+20);if(version>=3)memcpy(s->story,b+224,STORY_STATE_BYTES);
    if(!checkpoint_valid(s))return SAVE_INVALID;
    if(version==1 && (s->mode<8 || s->mode>10))return SAVE_INVALID;
    *seq=get32(b+12);return SAVE_OK;
}
/* Headers choose validation order only. Never trust their checksum or
 * payload until read_bank validates the entire selected file. An invalid
 * newer bank still falls back to the fully validated older bank. */
static uint32_t bank_sequence_hint(unsigned bank){
    service_io();char name[128];path(name,bank);FILE *f=fopen(name,"rb");
    if(!f)return 0;
    uint8_t header[20];size_t n=fread(header,1,sizeof(header),f);
    int error=ferror(f),closed=fclose(f);service_io();
    if(n!=sizeof(header) || error || closed || memcmp(header,magic,8) ||
       get32(header+8)<1 || get32(header+8)>4)return 0;
    return get32(header+12);
}
static int latest(GameCheckpoint *out,uint32_t *seq,unsigned *bank){
    uint32_t sa=bank_sequence_hint(0),sb=bank_sequence_hint(1);
    unsigned first=sb>sa;static GameCheckpoint candidate;uint32_t sequence=0;
    int a=read_bank(first,&candidate,&sequence);
    if(a==SAVE_OK){*out=candidate;*seq=sequence;*bank=first;return SAVE_OK;}
    int b=read_bank(!first,&candidate,&sequence);
    if(b==SAVE_OK){*out=candidate;*seq=sequence;*bank=!first;return SAVE_OK;}
    *seq=0;*bank=1;
    if(a==SAVE_MISSING && b==SAVE_MISSING)return SAVE_MISSING;
    if(a==SAVE_IO_ERROR || b==SAVE_IO_ERROR)return SAVE_IO_ERROR;
    return SAVE_INVALID;
}
int save_load(GameCheckpoint *out){
    if(!ready)return SAVE_NO_SD;
    static GameCheckpoint temp;uint32_t seq;unsigned bank;int r=latest(&temp,&seq,&bank);
    if(r==SAVE_OK){*out=temp;loaded_bank=bank;loaded_sequence=seq;loaded_slot=current_slot;}
    return r;
}
int save_restore_native(void){
    if(!ready)return SAVE_NO_SD;
    if(loaded_slot!=current_slot || !loaded_sequence)return SAVE_INVALID;
    char name[128];path(name,loaded_bank);FILE *f=fopen(name,"rb");if(!f)return SAVE_IO_ERROR;
    static uint8_t core[FILE_BYTES];uint8_t header[12];int result=SAVE_INVALID;
    uint8_t *data=NULL;unsigned bytes=0;
    if(fread(core,1,sizeof(core),f)<20){result=SAVE_IO_ERROR;goto done;}
    if(memcmp(core,magic,8) || get32(core+12)!=loaded_sequence)goto done;
    if(get32(core+8)<4){result=SAVE_MISSING;goto done;}
    if(get32(core+8)!=4 || !native_restore || fread(header,1,sizeof(header),f)!=sizeof(header) ||
       !(bytes=native_header(header)))goto done;
    data=malloc(bytes);if(!data){result=SAVE_IO_ERROR;goto done;}
    uint32_t expected=get32(core+16);put32(core+16,0);
    uint32_t crc=resource_crc_update(~0u,core+8,sizeof(core)-8);crc=resource_crc_update(crc,header,sizeof(header));
    for(unsigned at=0;at<bytes;){
        service_io();unsigned count=bytes-at;if(count>4096)count=4096;
        if(fread(data+at,1,count,f)!=count){result=SAVE_IO_ERROR;goto done;}
        crc=resource_crc_update(crc,data+at,count);at+=count;
    }
    if(fgetc(f)!=EOF || expected!=~crc)goto done;
    result=SAVE_OK;
done:
    {int error=ferror(f),closed=fclose(f);if(error || closed)result=SAVE_IO_ERROR;}
    /* Restoration can replace the room used by the I/O service. Complete
     * all file reads first; never keep a file callback across that change. */
    if(result==SAVE_OK && !native_restore(data,bytes))result=SAVE_INVALID;
    free(data);return result;
}
int save_peek_slot(unsigned slot,GameCheckpoint *out){
    if(slot<1 || slot>SAVE_SLOT_COUNT)return SAVE_INVALID;
    unsigned previous=current_slot;current_slot=slot;
    int result=save_load(out);current_slot=previous;return result;
}
int save_write(const GameCheckpoint *s){
    if(!ready)return SAVE_NO_SD;
    if(!checkpoint_valid(s))return SAVE_INVALID;
    uint8_t *native=NULL;size_t native_bytes=0;
    int captured=native_capture?native_capture(&native,&native_bytes):0;
    if(captured<0 || (captured==1 && (!native || !native_bytes || native_bytes>NATIVE_LIMIT)) ||
       (captured==0 && (native || native_bytes))){free(native);return SAVE_IO_ERROR;}
    static GameCheckpoint old;uint32_t seq;unsigned bank;
    int r=latest(&old,&seq,&bank);if(r==SAVE_IO_ERROR || seq==UINT32_MAX){free(native);return SAVE_IO_ERROR;}
#ifdef _WIN32
    int made=_mkdir(directory);
#else
    int made=mkdir(directory,0777);
#endif
    if(made!=0 && errno!=EEXIST){free(native);return SAVE_IO_ERROR;}
    service_io();
    static uint8_t b[FILE_BYTES];memcpy(b,magic,8);put32(b+8,captured?4:3);put32(b+12,seq+1);encode(b+20,s);put32(b+16,0);
    uint8_t header[12];memcpy(header,"NVRS",4);put32(header+4,1);put32(header+8,native_bytes);
    uint32_t crc=resource_crc_update(~0u,b+8,sizeof(b)-8);
    if(captured){crc=resource_crc_update(crc,header,sizeof(header));crc=resource_crc_update(crc,native,native_bytes);}
    put32(b+16,~crc);
    char name[128];path(name,!bank);FILE *f=fopen(name,"wb");if(!f){free(native);return SAVE_IO_ERROR;}
    service_io();
    bool good=fwrite(b,1,sizeof(b),f)==sizeof(b);service_io();
    if(good && captured){
        good=fwrite(header,1,sizeof(header),f)==sizeof(header);
        for(size_t at=0;good && at<native_bytes;){
            size_t count=native_bytes-at;if(count>4096)count=4096;
            good=fwrite(native+at,1,count,f)==count;at+=count;service_io();
        }
    }
    if(fflush(f))good=false;
    service_io();
    if(fclose(f))good=false;
    service_io();
    free(native);if(!good)return SAVE_IO_ERROR;
    static GameCheckpoint check;uint32_t written;return read_bank(!bank,&check,&written)==SAVE_OK && written==seq+1?SAVE_OK:SAVE_IO_ERROR;
}
