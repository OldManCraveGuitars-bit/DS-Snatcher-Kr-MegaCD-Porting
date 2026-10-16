#include <filesystem.h>
#include <nds.h>
#include <stdio.h>
extern size_t audio_serviced_fread(void *,size_t,size_t,FILE *);
#include <stdlib.h>
#include "portrait_motion.h"
#include "resource_portrait.h"
#include "resource_office_motion.h"
#include "../generated/voice_audio_r22.h"
#include "../generated/story_motion_r16.h"
#include "../generated/grant_motion.h"
#include "../generated/equipment_motion_r16.h"
#include "../generated/dispatch_motion.h"
#include "../generated/case_motion.h"
#include "../generated/combat_story_motion.h"

static void *original_resource;
static void *office_commands;
uint16_t portrait_images[3][4096] __attribute__((aligned(4)));
static uint16_t cached[3]={65535,65535,65535};
static unsigned first,count,current;
static bool active;
static bool native_office;
static unsigned last_samples,tail_fraction;
unsigned portrait_generation;
static bool initialize(void){
    if(original_resource)return true;
    if(!nitroFSInit(NULL))return false;
    FILE *file=fopen("nitro:/native-portraits-r22.nspr","rb");if(!file)return false;
    bool good=!fseek(file,0,SEEK_END);long bytes=good?ftell(file):-1;
    good=good && bytes>0 && bytes<=262144 && !fseek(file,0,SEEK_SET);
    void *data=good?malloc(bytes):NULL;
    good=data && audio_serviced_fread(data,1,bytes,file)==(size_t)bytes && !ferror(file);
    if(fclose(file))good=false;
    good=good && resource_portrait_bind(data,bytes) && resource_portrait_count()==COMBAT_STORY_FRAME_COUNT;
    if(!good){resource_portrait_bind(NULL,0);free(data);return false;}
    original_resource=data;return true;
}
static bool apply_frames(const uint16_t poses[3]){
    bool changed=false;
    for(unsigned slot=0;slot<3;slot++){
        unsigned frame=poses[slot];
        if(frame==cached[slot])continue;
        if(frame!=65535){
            if(frame>=COMBAT_STORY_FRAME_COUNT || !resource_portrait_draw(frame,portrait_images[slot]))return false;
        }
        cached[slot]=frame;
        changed=true;
    }
    if(changed)portrait_generation++;
    return true;
}
static bool initialize_office(void){
    if(office_commands)return true;
    FILE *file=fopen("nitro:/native-office-actors-r22.nsar","rb");if(!file)return false;
    bool good=!fseek(file,0,SEEK_END);long bytes=good?ftell(file):-1;
    good=good && bytes>0 && bytes<=262144 && !fseek(file,0,SEEK_SET);
    void *data=good?malloc(bytes):NULL;
    good=data && audio_serviced_fread(data,1,bytes,file)==(size_t)bytes && !ferror(file);
    if(fclose(file))good=false;
    good=good && resource_office_bind(data,bytes);
    if(!good){resource_office_bind(NULL,0);free(data);return false;}
    office_commands=data;return true;
}
static void leave_office(void){
    /* The original chief-call actor program is not used by later speakers.
     * Release its34KiB cache so the ordinary DS can allocate both audio
     * buffers before departure. It is reloaded for a later chief call. */
    native_office=false;
    if(office_commands){resource_office_bind(NULL,0);free(office_commands);office_commands=NULL;}
}
bool portrait_motion_begin(unsigned voice){
    if(voice>=VOICE_COUNT || !initialize() || !initialize_office())return false;
    uint16_t poses[3];active=native_office=true;last_samples=tail_fraction=0;
    return resource_office_begin(voice,poses) && apply_frames(poses);
}
bool portrait_motion_update(unsigned samples){
    if(!active)return true;
    unsigned divisor=((BUS_CLOCK/2)+(VOICE_RATE/2))/VOICE_RATE;
    unsigned physical_rate=BUS_CLOCK/(2*divisor);
    uint16_t poses[3];
    last_samples=samples;
    return resource_office_update(samples,physical_rate,poses) && apply_frames(poses);
}
bool portrait_motion_idle_tick(void){
    if(active || !native_office)return true;
    unsigned divisor=((BUS_CLOCK/2)+(VOICE_RATE/2))/VOICE_RATE;
    unsigned physical_rate=BUS_CLOCK/(2*divisor);
    uint64_t fraction=(uint64_t)tail_fraction+physical_rate*UINT64_C(1000000);
    last_samples+=fraction/59826098;tail_fraction=fraction%59826098;
    uint16_t poses[3];
    return resource_office_update(last_samples,physical_rate,poses) && apply_frames(poses);
}
bool portrait_story_begin(unsigned voice){
    leave_office();
    if(voice<77 || voice>85 || !initialize())return false;
    first=story_pose_starts[voice-77];count=story_pose_counts[voice-77];current=0;active=true;
    const uint16_t absent[3]={65535,65535,65535};
    return apply_frames(count?story_poses[first].poses:absent);
}
bool portrait_story_update(unsigned samples){
    if(!active || !count)return true;
    unsigned next=current;
    while(next+1<count && samples>=story_poses[first+next+1].sample)next++;
    if(next==current)return true;
    current=next;return apply_frames(story_poses[first+current].poses);
}
void portrait_motion_end(void){active=false;portrait_generation++;}
bool portrait_grant_begin(unsigned voice){
    leave_office();
    if(voice<95 || voice>102 || !initialize())return false;
    first=grant_pose_starts[voice-95];count=grant_pose_counts[voice-95];current=0;active=true;
    return apply_frames(grant_poses[first].poses);
}
bool portrait_grant_update(unsigned samples){
    if(!active)return true;
    unsigned next=current;
    while(next+1<count && samples>=grant_poses[first+next+1].sample)next++;
    if(next==current)return true;
    current=next;return apply_frames(grant_poses[first+current].poses);
}
const uint16_t *portrait_motion_image(unsigned slot){
    return slot<3 && cached[slot]!=65535?portrait_images[slot]:NULL;
}
bool portrait_equipment_begin(unsigned voice){
    leave_office();
    if(voice<103 || voice>120 || !initialize())return false;
    first=equipment_pose_starts[voice-103];count=equipment_pose_counts[voice-103];current=0;active=true;
    const uint16_t absent[3]={65535,65535,65535};
    return apply_frames(count?equipment_poses[first].poses:absent);
}
bool portrait_equipment_update(unsigned samples){
    if(!active || !count)return true;
    unsigned next=current;
    while(next+1<count && samples>=equipment_poses[first+next+1].sample)next++;
    if(next==current)return true;
    current=next;return apply_frames(equipment_poses[first+current].poses);
}
bool portrait_dispatch_begin(unsigned voice){
    leave_office();
    if(voice<121 || voice>136 || !initialize())return false;
    first=dispatch_pose_starts[voice-121];count=dispatch_pose_counts[voice-121];current=0;active=true;
    const uint16_t absent[3]={65535,65535,65535};
    return apply_frames(count?dispatch_poses[first].poses:absent);
}
bool portrait_dispatch_update(unsigned samples){
    if(!active || !count)return true;
    unsigned next=current;
    while(next+1<count && samples>=dispatch_poses[first+next+1].sample)next++;
    if(next==current)return true;
    current=next;return apply_frames(dispatch_poses[first+current].poses);
}
bool portrait_case_begin(unsigned voice){
    leave_office();
    if(voice<137 || voice>143 || !initialize())return false;
    first=case_pose_starts[voice-137];count=case_pose_counts[voice-137];current=0;active=true;
    const uint16_t absent[3]={65535,65535,65535};
    return apply_frames(count?case_poses[first].poses:absent);
}
bool portrait_case_update(unsigned samples){
    if(!active || !count)return true;
    unsigned next=current;
    while(next+1<count && samples>=case_poses[first+next+1].sample)next++;
    if(next==current)return true;
    current=next;return apply_frames(case_poses[first+current].poses);
}

bool portrait_combat_story_begin(unsigned voice){
    leave_office();
    if(voice<144 || voice>169 || !initialize())return false;
    first=combat_story_pose_starts[voice-144];count=combat_story_pose_counts[voice-144];current=0;active=true;
    const uint16_t absent[3]={65535,65535,65535};
    return apply_frames(count?combat_story_poses[first].poses:absent);
}
bool portrait_combat_story_update(unsigned samples){
    if(!active || !count)return true;
    unsigned next=current;
    while(next+1<count && samples>=combat_story_poses[first+next+1].sample)next++;
    if(next==current)return true;
    current=next;return apply_frames(combat_story_poses[first+current].poses);
}

bool portrait_live_active;
void portrait_live_mode(bool enabled){
    if(portrait_live_active==enabled)return;
    portrait_live_active=enabled;active=false;leave_office();
    for(unsigned i=0;i<3;i++)cached[i]=65535;
    if(enabled && original_resource){resource_portrait_bind(NULL,0);free(original_resource);original_resource=NULL;}
    (void)portrait_live_source(NULL,0);portrait_generation++;
}
