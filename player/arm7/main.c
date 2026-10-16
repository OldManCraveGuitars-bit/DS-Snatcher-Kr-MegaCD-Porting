// SPDX-License-Identifier: Zlib
// SPDX-FileNotice: Modified from the original version by the BlocksDS project.
//
// Copyright (C) 2005 Michael Noland (joat)
// Copyright (C) 2005 Jason Rogers (Dovoto)
// Copyright (C) 2005-2015 Dave Murphy (WinterMute)
// Copyright (C) 2023 Antonio Niño Díaz

// Default ARM7 core

#include <nds.h>


#if defined(USE_MAXMOD) && defined(USE_LIBXM7)
#error "Only one audio library can be used"
#endif

#if defined(USE_DSWIFI)
#include <dswifi7.h>
#endif

#if defined(USE_MAXMOD)
#include <maxmod7.h>
#endif

#if defined(USE_LIBXM7)
#include <libxm7.h>

// Assign FIFO_USER_07 channel to LibXM7
#define FIFO_LIBXM7 FIFO_USER_07

void XM7_Value32Handler(u32 command, void *userdata)
{
    (void)userdata;

    XM7_ModuleManager_Type *module = (XM7_ModuleManager_Type *)command;

    if (module == NULL)
        XM7_StopModule();
    else
        XM7_PlayModule(module);
}
#endif


#include <string.h>
#include "fm_parallel.h"
UINT16 ym_lookup[256+5504];
static UINT16 row_data[16][256],quiet_row[256];
static UINT8 row_map[416];
static UINT16 row_keys[16];
static unsigned row_cursor;
static const UINT16 *local_row(unsigned volume,unsigned *pinned){
    if(volume>=832)return quiet_row;
    unsigned key=volume>>1,index=row_map[key];
    if(index)index--;
    else{
        index=row_cursor;
        while(*pinned&(1u<<index))index=(index+1)&15;
        if(row_keys[index])row_map[row_keys[index]-1]=0;
        for(unsigned x=0;x<256;x++)row_data[index][x]=ym_lookup[256+ym_lookup[x]+(key<<3)];
        row_keys[index]=key+1;row_map[key]=index+1;row_cursor=(index+1)&15;
    }
    *pinned|=1u<<index;return row_data[index];
}
static void prepare_local_rows(chan_rend_context *ct){
    if(!ct->stable_loop)return;
    unsigned pinned=0;
    ct->stable_rows[0]=local_row(ct->vol_out1,&pinned);
    ct->stable_rows[1]=local_row(ct->vol_out2,&pinned);
    ct->stable_rows[2]=local_row(ct->vol_out3,&pinned);
    ct->stable_rows[3]=local_row(ct->vol_out4,&pinned);
}
static fm_shared *volatile queue;
static channel_job local_job;
extern void chan_render_loop(chan_rend_context *,s32 *,unsigned short);
static void fm_queue_address(void *address,void *unused){
    (void)unused;queue=address;
    memcpy(ym_lookup,queue->lookup,sizeof(ym_lookup));
    memset(row_map,0,sizeof(row_map));memset(row_keys,0,sizeof(row_keys));row_cursor=0;
    queue->ready=1;
}
static void render_local_spans(chan_rend_context *ct,s32 *buffer,unsigned length){
    if(ct->stable_loop || (ct->pack&14) || ct->eg_timer_add!=65536 ||
       ct->eg_timer>=65536 || ct->eg_cnt>=4096 || (ct->algo&0x40)){
        chan_render_loop(ct,buffer,length);return;
    }
    for(unsigned i=0;i<4;i++){
        const FM_SLOT *slot=&ct->CH->SLOT[i];
        if(slot->state && slot->state<=4){
            unsigned pack=slot->eg_pack[slot->state-1];
            if((pack&0xffffff) && (pack>>24)<3){chan_render_loop(ct,buffer,length);return;}
        }
    }
    unsigned active=0;
    while(length){
        unsigned first=(ct->algo>>4)&3;if(!first)first=1;
        unsigned quiet=length;
        for(unsigned i=0;i<4;i++){
            const FM_SLOT *slot=&ct->CH->SLOT[i];
            if(!slot->state)continue;
            unsigned pack=slot->eg_pack[slot->state-1],shift=pack>>24;
            if(slot->state==2 && !(pack&0xffffff))continue;
            if(shift>=12)continue;
            unsigned period=1u<<shift;
            unsigned distance=period-(ct->eg_cnt&(period-1));
            if(ct->eg_cnt+distance>=4096)distance=4095-ct->eg_cnt+period;
            unsigned before=first+(distance-1)*3-1;
            if(before<quiet)quiet=before;
        }
        unsigned take;
        if(quiet>=8){
            take=quiet;
            unsigned updates=take<first?0:1+(take-first)/3;
            unsigned remaining=take<first?first-take:3-(take-first)%3;
            ct->eg_cnt+=updates;if(ct->eg_cnt>=4096)ct->eg_cnt-=4095;
            ct->algo=(ct->algo&~0x30u)|(remaining<<4);
            ct->vol_out1=ct->CH->SLOT[0].vol_out;
            ct->vol_out2=ct->CH->SLOT[2].vol_out;
            ct->vol_out3=ct->CH->SLOT[1].vol_out;
            ct->vol_out4=ct->CH->SLOT[3].vol_out;
            ct->stable_loop=1;
            prepare_local_rows(ct);
        }else{
            take=quiet<length?quiet+1:length;ct->stable_loop=0;
        }
        chan_render_loop(ct,buffer,take);active|=ct->algo&8;
        buffer+=take*2;length-=take;
    }
    ct->algo|=active;
}
static void fm_worker_wake(u32 value,void *unused){
    (void)value;(void)unused;
}
static void fm_process_jobs(void){
    if(!queue)return;
    queue->worker_phase=1;
    for(unsigned c=0;c<2;c++){
        channel_job *job=&queue->jobs[c];if(job->active!=1)continue;
        unsigned count=job->length;
        if(!count || count>128){queue->ready=2;queue->worker_phase=9;continue;}
        queue->worker_started++;queue->worker_phase=2;
        local_job.context=job->context;local_job.channel=job->channel;
        local_job.context.CH=&local_job.channel;
        prepare_local_rows(&local_job.context);
        memset(local_job.output,0,count*2*sizeof(s32));
        render_local_spans(&local_job.context,local_job.output,count);
        local_job.context.CH=NULL;
        job->context=local_job.context;job->channel=local_job.channel;
        memcpy(job->output,local_job.output,count*2*sizeof(s32));job->active=2;
        queue->worker_completed++;queue->worker_phase=3;
    }
}

volatile bool exit_loop = false;

void power_button_callback(void)
{
    exit_loop = true;
}

void vblank_handler(void)
{
    inputGetAndSend();
#if defined(USE_DSWIFI)
    Wifi_Update();
#endif
}

int main(void)
{
#if defined(DEBUG_LIBS)
    // The ARM9 exception handler can trap data aborts and undefined instruction
    // exceptions. However, the ARM7 handler can only trap undefined
    // instructions. When an exception happens, a message is sent to the ARM9 to
    // display the crash information.
    defaultExceptionHandler();
#endif

    // Initialize sound hardware
    // Required for some functions within libnds
    enableSound();

    // Read user information from the firmware (name, birthday, etc)
    readUserSettings();

    // Stop LED blinking
    ledBlink(0);

    // Using the calibration values read from the firmware with
    // readUserSettings(), calculate some internal values to convert raw
    // coordinates into screen coordinates.
    touchInit();

    irqInit();
    fifoInit();

#if defined(USE_DSWIFI)
    installWifiFIFO();
#endif

    // Required for some functions within libnds
    installSoundFIFO();

    installSystemFIFO(); // Sleep mode, storage, firmware...
    if (isDSiMode())
        installCameraFIFO();

#if defined(USE_MAXMOD)
    // Initialize Maxmod. It uses timer 0 internally.
    mmInstall(FIFO_MAXMOD);
#endif

#if defined(USE_LIBXM7)
    // Initialize LibXM7. It uses timer 0 internally.
    XM7_Initialize();
    // Setup the FIFO handler for LibXM7
    fifoSetValue32Handler(FIFO_LIBXM7, XM7_Value32Handler, 0);
#endif

    // This sets a callback that is called when the power button in a DSi
    // console is pressed. It has no effect in a DS.
    setPowerButtonCB(power_button_callback);

    // Read current date from the RTC and setup an interrupt to update the time
    // regularly. The interrupt simply adds one second every time, it doesn't
    // read the date. Reading the RTC is very slow, so it's a bad idea to do it
    // frequently.
    initClockIRQTimer(LIBNDS_DEFAULT_TIMER_RTC);

    // Now that the FIFO is setup we can start sending input data to the ARM9.
    irqSet(IRQ_VBLANK, vblank_handler);
    irqEnable(IRQ_VBLANK);
    fifoSetAddressHandler(FIFO_USER_01,fm_queue_address,NULL);
    fifoSetValue32Handler(FIFO_USER_02,fm_worker_wake,NULL);

    while (!exit_loop)
    {
        const uint16_t key_mask = KEY_SELECT | KEY_START | KEY_L | KEY_R;
        uint16_t keys_pressed = ~REG_KEYINPUT;

        if ((keys_pressed & key_mask) == key_mask)
            exit_loop = true;

        fm_process_jobs();
        swiIntrWait(INTRWAIT_KEEP_FLAGS,IRQ_RECV_FIFO|IRQ_VBLANK);
    }

    return 0;
}
