/* Private integration of ordinary game requests into the single original
 * MAIN/YM + SUB PCM device. Native entry/flight still owns its older device;
 * that boundary must be joined before this candidate can be promoted. */
static unsigned shared_frame_at(uint64_t samples){
    uint64_t clocks=samples*53693175u;
    return clocks/(896040ull*32000u);
}
static void *shared_read(const char *path,unsigned *bytes){
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    bool good=!fseek(f,0,SEEK_END);long n=good?ftell(f):-1;
    good=good && n>0 && n<=1048576 && !fseek(f,0,SEEK_SET);
    void *p=good?malloc(n):NULL;
    if(p && fread(p,1,n,f)!=(unsigned)n){free(p);p=NULL;}
    fclose(f);if(p)*bytes=n;return p;
}
volatile unsigned shared_played_fault;
static bool shared_begin(void){
    if(shared_audio)return !music_error;
    if(entry)return false;
    /* Preserve a PCM bank loaded by a restored room before its score is
     * requested. Never retain the old object's internal storage pointer. */
    if(streaming)mmStreamClose();streaming=false;
    resource_fm_close();resource_music_close(&bank);resource_music_close(&effect_bank);
    resource_music_close(&arrival_bank);resource_music_close(&forecourt_bank);
    resource_room_audio_drums_end(room_audio);free(drum_commands);drum_commands=NULL;
    FILE *f=fopen("nitro:/native-room-pcm-r22.nrs","rb");if(!f)return false;
    if(!room_table)room_table=resource_archive_load(f,"PCMLT_01.BIN",&room_table_bytes);
    if(!room_resident)room_resident=resource_archive_load(f,"PCMDRMDT.BIN",&room_resident_bytes);
    fclose(f);
    unsigned ni=0,nf=0;
    void *ip=shared_read("nitro:/native-driver-4c-ip.bin",&ni);
    void *fm=shared_read("nitro:/native-driver-4c-fm.bin",&nf);
    resource_shared_sound *next=malloc(sizeof(*next));
    resource_pcm_effect_bank input={room_table,room_resident,NULL,room_table_bytes,room_resident_bytes,0};
    bool good=next && ip && fm && resource_shared_sound_begin(next,&input,ip,ni,fm,nf);
    free(ip);free(fm);
    if(!good){resource_shared_sound_end(next);free(next);return false;}
    if(room_audio){
        next->pcm=*room_audio;
        if(room_audio->driver.bank.dynamic==room_audio->driver.storage)
            next->pcm.driver.bank.dynamic=next->pcm.driver.storage;
        free(room_audio);
    }
    shared_audio=next;shared_stream_origin=0;room_audio=&next->pcm;
    music_written=music_played=music_error=music_missing=music_loops=0;
    music_start_sample=0;music_min_lead=music_min_fill_lead=0x7fffffff;
    shared_wait_active=false;shared_wait_remaining=0;
    shared_game_requests=shared_game_waits=shared_game_completions=shared_game_timeouts=0;
    shared_played_fault=0;shared_game_epoch++;shared_game_event_count=shared_game_fill_count=0;
    if(music_command){
        if(!resource_shared_sound_request(next,1,music_command,1))return false;
        shared_event(1,music_command,1);shared_game_requests++;
        if(!shared_start_stream())return false;
    }
    return true;
}
static bool shared_played(void){
    if(!shared_audio || !streaming)return true;
    uint64_t played=shared_stream_origin+mmStreamGetPosition();
    if(played>shared_audio->samples){shared_played_fault=1;music_error=8;return false;}
    if(!resource_shared_sound_played(shared_audio,played)){shared_played_fault=2;
        music_error=8;return false;
    }
    shared_game_status=(unsigned)shared_audio->music<<16|(unsigned)shared_audio->effect<<8|shared_audio->sequence;
    return true;
}
static bool shared_start_stream(void){
    if(streaming)return !music_error;
    if(!music_player_init())return false;
    mm_stream stream={32000,entry?(flight?NATIVE_FLIGHT_BUFFER_FRAMES:NATIVE_ENTRY_BUFFER_FRAMES):MUSIC_STREAM_BUFFER_FRAMES,fill,MM_STREAM_16BIT_STEREO,MM_TIMER2,true};
    mmStreamOpen(&stream);
    if(!music_written){music_error=7;return false;}
    music_player_stream_ready();
    return !music_error;
}
static bool shared_request(unsigned command,unsigned channel){
    if(command>255 || channel>1 || !shared_audio)return false;
    /* Schedule immediately after the generated source tick, without dropping
     * any already-produced DAC samples. Existing look-ahead latency remains
     * visible and must be reduced before production promotion. */
    unsigned tick=shared_audio->generated_frame+1;
    if(!resource_shared_sound_request(shared_audio,tick,command,channel))return false;
    shared_event(channel,command,tick);
    shared_game_requests++;
    if(channel){music_command=command==255?0:command;music_start_sample=shared_audio->samples;}
    return true;
}
static bool shared_command(unsigned command,unsigned channel){
    return shared_begin() && shared_request(command,channel) && shared_start_stream();
}
static bool shared_render(int16_t *out,unsigned frames){
    if(!frames)return true;
#ifdef SNATCHER_AUDIO_HASH
    uint32_t hash;
    if(!shared_played() || !resource_shared_sound_render_observed(shared_audio,out,frames,&hash))return false;
    unsigned slot=shared_game_fill_count&4095;
    shared_game_fills[slot]=frames;shared_game_hashes[slot]=hash;
#else
    if(!shared_played() || !resource_shared_sound_render(shared_audio,out,frames))return false;
#endif
    shared_game_fill_count++;
    /* Prefix reconstruction predates the restored DAC origin. Consume its
     * source notifications as that past advances, rather than retaining
     * hundreds of already-past VBlanks in the live look-ahead queue. This
     * path is forbidden while the actual DAC is running. */
    if(shared_reconstructing && (streaming ||
       !resource_shared_sound_played(shared_audio,shared_audio->samples)))return false;
    return entry || resource_cdda_mix(&story_cd,out,frames,music_mix_scratch,256);
}
static bool entry_shared_render(void *c,int16_t *out,unsigned n){(void)c;return shared_render(out,n);}
static bool entry_shared_command(void *c,unsigned command,unsigned channel){(void)c;return shared_request(command,channel);}
static void entry_shared_status(void *c,uint8_t *music,uint8_t *effect){
    (void)c;resource_shared_sound_generated_status(shared_audio,music,effect);
}
static void entry_shared_event(void *c,unsigned kind,unsigned a,unsigned b){(void)c;shared_event(kind,a,b);}
static bool shared_sound_wait_begin(bool music){
    if(!shared_audio || !streaming || shared_wait_active || !shared_played())return false;
    shared_wait_frame=shared_frame_at(shared_audio->played);
    shared_wait_remaining=music?2700:600;shared_wait_music=music;shared_wait_active=true;shared_game_waits++;return true;
}
bool music_player_effect_wait_begin(void){return shared_sound_wait_begin(false);}
bool music_player_music_wait_begin(void){return shared_sound_wait_begin(true);}
int music_player_sound_wait_status(void){
    if(!shared_wait_active)return 0;
    if(!shared_played())return -1;
    unsigned now=shared_frame_at(shared_audio->played),elapsed=now-shared_wait_frame;
    if(!elapsed)return 1;
    /* Original SUB103A2 decrements before testing effect completion. This is
     * its fail-safe, not a fabricated duration. UI frames are not the clock. */
    shared_wait_frame=now;
    if(elapsed>=shared_wait_remaining){
        shared_wait_remaining=0;shared_wait_active=false;shared_game_timeouts++;return 0;
    }
    shared_wait_remaining-=elapsed;
    if(!(shared_wait_music?shared_audio->music:shared_audio->effect)){shared_wait_active=false;shared_game_completions++;return 0;}
    return 1;
}
