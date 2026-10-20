/* Private portable YM checkpoint codec. Included after the pinned YM core.
 * Source-derived initial states only: no emulator RAM or audio capture input.
 * No pointer, native struct padding, or compiler ABI bytes enter the wire file.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct {uint8_t *write;const uint8_t *read;unsigned at,bytes;int ok;} fm_state_io;
static uint32_t fm_state_word(fm_state_io *io,uint32_t value){
    if(io->at>io->bytes || io->bytes-io->at<4){io->ok=0;return 0;}
    if(io->read){const uint8_t *p=io->read+io->at;value=p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
    if(io->write)for(unsigned j=0;j<4;j++)io->write[io->at+j]=(uint8_t)(value>>(j*8));
    io->at+=4;return value;
}
#include "resource_checksum.h"
static uint32_t fm_state_crc(const void *data,unsigned bytes){return resource_crc32(data,bytes);}
#define FM_FIELD(x) do {uint32_t value=fm_state_word(io,(uint32_t)(x));if(io->read){(x)=value;if((uint32_t)(x)!=value)io->ok=0;}}while(0)
static int fm_state_fields(fm_state_io *io,YM2612 *chip,chan_rend_context *context){
    for(unsigned i=0;i<512;i++)FM_FIELD(chip->REGS[i]);
    FM_FIELD(chip->addr_A1);FM_FIELD(chip->dacen);FM_FIELD(chip->dacout);
    FM_FIELD(chip->slot_mask);FM_FIELD(chip->ssg_mask);
    for(unsigned channel=0;channel<6;channel++){
        FM_CH *ch=&chip->CH[channel];
        for(unsigned slot=0;slot<4;slot++){
            FM_SLOT *s=&ch->SLOT[slot];unsigned detune=8;
            if(!io->read)for(unsigned i=0;i<8;i++)if(s->DT==ym2612.OPN.ST.dt_tab[i])detune=i;
            detune=fm_state_word(io,detune);if(detune>=8)io->ok=0;
            if(io->read && detune<8)s->DT=ym2612.OPN.ST.dt_tab[detune];
            FM_FIELD(s->ar);FM_FIELD(s->d1r);FM_FIELD(s->d2r);FM_FIELD(s->rr);FM_FIELD(s->mul);
            FM_FIELD(s->phase);FM_FIELD(s->Incr);FM_FIELD(s->KSR);FM_FIELD(s->ksr);FM_FIELD(s->key);
            FM_FIELD(s->state);FM_FIELD(s->tl);FM_FIELD(s->volume);FM_FIELD(s->sl);
            for(unsigned e=0;e<4;e++)FM_FIELD(s->eg_pack[e]);
            FM_FIELD(s->ssg);FM_FIELD(s->ssgn);FM_FIELD(s->ar_ksr);FM_FIELD(s->vol_out);
        }
        FM_FIELD(ch->ALGO);FM_FIELD(ch->FB);FM_FIELD(ch->op1_out);FM_FIELD(ch->mem_value);
        FM_FIELD(ch->pms);FM_FIELD(ch->ams);FM_FIELD(ch->kcode);FM_FIELD(ch->upd_cnt);
        FM_FIELD(ch->fc);FM_FIELD(ch->block_fnum);FM_FIELD(ch->AMmasks);
    }
    FM_ST *st=&chip->OPN.ST;
    FM_FIELD(st->clock);FM_FIELD(st->rate);
    /* All supported probes initialize this exact native synthesis ratio.
     * Immutable pitch/detune tables are initialized normally on the target. */
    if(st->clock!=144000 || st->rate!=1000 || st->freqbase!=1.0)io->ok=0;
    FM_FIELD(st->address);FM_FIELD(st->status);FM_FIELD(st->mode);FM_FIELD(st->flags);
    FM_FIELD(st->TA);FM_FIELD(st->TB);FM_FIELD(st->fn_h);
    for(unsigned t=0;t<8;t++)for(unsigned v=0;v<32;v++)FM_FIELD(st->dt_tab[t][v]);
    FM_3SLOT *sl=&chip->OPN.SL3;
    for(unsigned i=0;i<3;i++){FM_FIELD(sl->fc[i]);FM_FIELD(sl->kcode[i]);FM_FIELD(sl->block_fnum[i]);}
    FM_FIELD(sl->fn_h);
    FM_OPN *opn=&chip->OPN;
    FM_FIELD(opn->pan);FM_FIELD(opn->eg_cnt);FM_FIELD(opn->eg_timer);FM_FIELD(opn->eg_timer_add);
    FM_FIELD(opn->lfo_cnt);FM_FIELD(opn->lfo_inc);FM_FIELD(opn->lfo_ampm);
    for(unsigned i=0;i<8;i++)FM_FIELD(opn->lfo_freq[i]);
    /* The upstream renderer retains EG/LFO context when all channels are
     * quiet. Preserve that scalar context too, including its logical channel. */
    unsigned ch_index=6;
    if(!io->read)for(unsigned i=0;i<6;i++)if(context->CH==&ym2612.CH[i])ch_index=i;
    ch_index=fm_state_word(io,ch_index);if(ch_index>6)io->ok=0;
    if(io->read)context->CH=ch_index<6?&ym2612.CH[ch_index]:NULL;
    FM_FIELD(context->vol_out1);FM_FIELD(context->vol_out2);FM_FIELD(context->vol_out3);FM_FIELD(context->vol_out4);
    FM_FIELD(context->lfo_init_sft16);
    FM_FIELD(context->phase1);FM_FIELD(context->phase2);FM_FIELD(context->phase3);FM_FIELD(context->phase4);
    FM_FIELD(context->incr1);FM_FIELD(context->incr2);FM_FIELD(context->incr3);FM_FIELD(context->incr4);
    FM_FIELD(context->lfo_cnt);FM_FIELD(context->lfo_inc);FM_FIELD(context->mem);FM_FIELD(context->eg_cnt);
    FM_FIELD(context->eg_timer);FM_FIELD(context->eg_timer_add);FM_FIELD(context->pack);
    FM_FIELD(context->algo);FM_FIELD(context->op1_out);
    /* Optimizer cache pointers are recomputed on the next channel block. */
    if(io->read){context->stable_loop=0;context->stable_envelopes=0;memset(context->stable_rows,0,sizeof(context->stable_rows));}
    return io->ok;
}
#undef FM_FIELD
static unsigned fm_chip_export_context(void *data,unsigned capacity,chan_rend_context *context){
    if(!data || capacity<16)return 0;
    fm_state_io io={(uint8_t*)data,NULL,16,capacity,1};
    if(!fm_state_fields(&io,&ym2612,context))return 0;
    unsigned size=io.at;io.at=0;
    fm_state_word(&io,0x50434d46u);fm_state_word(&io,1);fm_state_word(&io,size);
    fm_state_word(&io,fm_state_crc((uint8_t*)data+16,size-16));return size;
}
unsigned fm_chip_export(void *data,unsigned capacity){return fm_chip_export_context(data,capacity,&crct);}
unsigned fm_chip_export_portable(void *data,unsigned capacity){
    /* ARM loops pack their temporary sample countdown above bit7. All
     * renderers load only algo&0xf7, and commits use bits0..7. Preserve the
     * original legacy codec; canonical shared saves omit only that exhausted
     * loop counter, without changing live envelope/phase/feedback state. */
    chan_rend_context portable=crct;portable.algo&=255;
    return fm_chip_export_context(data,capacity,&portable);
}
int fm_chip_import(const void *data,unsigned bytes){
    if(!data || bytes<16)return 0;
    fm_state_io io={NULL,(const uint8_t*)data,0,bytes,1};
    if(fm_state_word(&io,0)!=0x50434d46u || fm_state_word(&io,0)!=1 || fm_state_word(&io,0)!=bytes)return 0;
    if(fm_state_word(&io,0)!=fm_state_crc((const uint8_t*)data+16,bytes-16))return 0;
    /* Decode transactionally without consuming the DS user stack. */
    static YM2612 pending_chip;static chan_rend_context pending_context;
    pending_chip=ym2612;pending_context=crct;
    if(!fm_state_fields(&io,&pending_chip,&pending_context) || io.at!=bytes)return 0;
    ym2612=pending_chip;crct=pending_context;return 1;
}
