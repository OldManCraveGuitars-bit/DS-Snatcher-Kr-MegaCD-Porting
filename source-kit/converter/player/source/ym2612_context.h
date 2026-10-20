#ifndef YM_CONTEXT_H
#define YM_CONTEXT_H
#include "ym2612.h"
typedef struct
{
	UINT16 vol_out1; /* 00: current output from EG circuit (without AM from LFO) */
	UINT16 vol_out2;
	UINT16 vol_out3;
	UINT16 vol_out4;
	UINT32 lfo_init_sft16;
	UINT32 pad;
	UINT32 phase1;   /* 10 */
	UINT32 phase2;
	UINT32 phase3;
	UINT32 phase4;
	UINT32 incr1;    /* 20: phase step */
	UINT32 incr2;
	UINT32 incr3;
	UINT32 incr4;
	UINT32 lfo_cnt;  /* 30 */
	UINT32 lfo_inc;
	INT32  mem;      /* one sample delay memory */
	UINT32 eg_cnt;   /* envelope generator counter */
	FM_CH  *CH;      /* 40: envelope generator counter */
	UINT32 eg_timer;
	UINT32 eg_timer_add;
	UINT32 pack;     // 4c: stereo, lastchan, disabled, lfo_enabled | pan_r, pan_l, ams[2] | AMmasks[4] | FB[4] | lfo_ampm[16]
	UINT32 algo;     /* 50: algo[3], was_update, unused, upd_cnt[2], dac */
	INT32  op1_out;
#ifdef _MIPS_ARCH_ALLEGREX
	UINT32 pad1[3+8];
#endif
    UINT32 stable_envelopes;
    UINT32 stable_loop;
    const UINT16 *stable_rows[4];
} chan_rend_context;

#endif
