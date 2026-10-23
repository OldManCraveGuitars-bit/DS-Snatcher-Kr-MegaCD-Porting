#include "system_perf.h"
/*
** This is a bunch of remains of original fm.c from MAME project. All stuff
** unrelated to ym2612 was removed, multiple chip support was removed,
** some parts of code were slightly rewritten and tied to the emulator.
**
** SSG-EG was also removed, because it's rarely used, Sega2.doc even does not
** document it ("proprietary") and tells to write 0 to SSG-EG control register.
**
** updated with fixes from mame 0.216 (file version 1.5.1) (kub)
** SSG-EG readded from GenPlus (kub)
*/

/*
**
** File: fm.c -- software implementation of Yamaha FM sound generator
**
** Copyright (C) 2001, 2002, 2003 Jarek Burczynski (bujar at mame dot net)
** Copyright (C) 1998 Tatsuyuki Satoh , MultiArcadeMachineEmulator development
**
** Version 1.4 (final beta)
**
*/

/*
** History:
**
** 03-08-2003 Jarek Burczynski:
**  - fixed YM2608 initial values (after the reset)
**  - fixed flag and irqmask handling (YM2608)
**  - fixed BUFRDY flag handling (YM2608)
**
** 14-06-2003 Jarek Burczynski:
**  - implemented all of the YM2608 status register flags
**  - implemented support for external memory read/write via YM2608
**  - implemented support for deltat memory limit register in YM2608 emulation
**
** 22-05-2003 Jarek Burczynski:
**  - fixed LFO PM calculations (copy&paste bugfix)
**
** 08-05-2003 Jarek Burczynski:
**  - fixed SSG support
**
** 22-04-2003 Jarek Burczynski:
**  - implemented 100% correct LFO generator (verified on real YM2610 and YM2608)
**
** 15-04-2003 Jarek Burczynski:
**  - added support for YM2608's register 0x110 - status mask
**
** 01-12-2002 Jarek Burczynski:
**  - fixed register addressing in YM2608, YM2610, YM2610B chips. (verified on real YM2608)
**    The addressing patch used for early Neo-Geo games can be removed now.
**
** 26-11-2002 Jarek Burczynski, Nicola Salmoria:
**  - recreated YM2608 ADPCM ROM using data from real YM2608's output which leads to:
**  - added emulation of YM2608 drums.
**  - output of YM2608 is two times lower now - same as YM2610 (verified on real YM2608)
**
** 16-08-2002 Jarek Burczynski:
**  - binary exact Envelope Generator (verified on real YM2203);
**    identical to YM2151
**  - corrected 'off by one' error in feedback calculations (when feedback is off)
**  - corrected connection (algorithm) calculation (verified on real YM2203 and YM2610)
**
** 18-12-2001 Jarek Burczynski:
**  - added SSG-EG support (verified on real YM2203)
**
** 12-08-2001 Jarek Burczynski:
**  - corrected ym_sin_tab and ym_tl_tab data (verified on real chip)
**  - corrected feedback calculations (verified on real chip)
**  - corrected phase generator calculations (verified on real chip)
**  - corrected envelope generator calculations (verified on real chip)
**  - corrected FM volume level (YM2610 and YM2610B).
**  - changed YMxxxUpdateOne() functions (YM2203, YM2608, YM2610, YM2610B, YM2612) :
**    this was needed to calculate YM2610 FM channels output correctly.
**    (Each FM channel is calculated as in other chips, but the output of the channel
**    gets shifted right by one *before* sending to accumulator. That was impossible to do
**    with previous implementation).
**
** 23-07-2001 Jarek Burczynski, Nicola Salmoria:
**  - corrected YM2610 ADPCM type A algorithm and tables (verified on real chip)
**
** 11-06-2001 Jarek Burczynski:
**  - corrected end of sample bug in ADPCMA_calc_cha().
**    Real YM2610 checks for equality between current and end addresses (only 20 LSB bits).
**
** 08-12-98 hiro-shi:
** rename ADPCMA -> ADPCMB, ADPCMB -> ADPCMA
** move ROM limit check.(CALC_CH? -> 2610Write1/2)
** test program (ADPCMB_TEST)
** move ADPCM A/B end check.
** ADPCMB repeat flag(no check)
** change ADPCM volume rate (8->16) (32->48).
**
** 09-12-98 hiro-shi:
** change ADPCM volume. (8->16, 48->64)
** replace ym2610 ch0/3 (YM-2610B)
** change ADPCM_SHIFT (10->8) missing bank change 0x4000-0xffff.
** add ADPCM_SHIFT_MASK
** change ADPCMA_DECODE_MIN/MAX.
*/




/************************************************************************/
/*    comment of hiro-shi(Hiromitsu Shioya)                             */
/*    YM2610(B) = OPN-B                                                 */
/*    YM2610  : PSG:3ch FM:4ch ADPCM(18.5KHz):6ch DeltaT ADPCM:1ch      */
/*    YM2610B : PSG:3ch FM:6ch ADPCM(18.5KHz):6ch DeltaT ADPCM:1ch      */
/************************************************************************/

//#include <stdio.h>

#include <assert.h>
#include <string.h>
#include <math.h>

#include "probe_types.h"
#include "ym2612.h"
#include "audio_hot_state.h"
#ifdef __NDS__
#include <nds.h>
#else
#define ITCM_CODE
#define DTCM_DATA
#endif

#ifndef EXTERNAL_YM2612
#include <stdlib.h>
// let it be 1 global to simplify things
YM2612 ym2612 __attribute__((aligned(32)));

#else
extern YM2612 *ym2612_940;
#define ym2612 (*ym2612_940)

#endif

void memset32(void *dest, int c, int count);


#ifndef __GNUC__
#pragma warning (disable:4100) // unreferenced formal parameter
#pragma warning (disable:4244)
#pragma warning (disable:4245) // signed/unsigned in conversion
#pragma warning (disable:4710)
#pragma warning (disable:4018) // signed/unsigned
#endif

#ifndef INLINE
#define INLINE __inline
#endif

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif


/* globals */

#define FREQ_SH			16  /* 16.16 fixed point (frequency calculations) */
#define EG_SH			16  /* 16.16 fixed point (envelope generator timing) */
#define LFO_SH			24  /*  8.24 fixed point (LFO calculations)       */

#define ENV_BITS		10
#define ENV_LEN			(1<<ENV_BITS)
#define ENV_STEP		(128.0/ENV_LEN)

#define MAX_ATT_INDEX	(ENV_LEN-1) /* 1023 */
#define MIN_ATT_INDEX	(0)			/* 0 */

#define EG_ATT			4
#define EG_DEC			3
#define EG_SUS			2
#define EG_REL			1
#define EG_OFF			0

#define SIN_BITS		10
#define SIN_LEN			(1<<SIN_BITS)
#define SIN_MASK		(SIN_LEN-1)

#define TL_RES_LEN		(256) /* 8 bits addressing (real chip) */

#define EG_TIMER_OVERFLOW (3*(1<<EG_SH)) /* envelope generator timer overflows every 3 samples (on real chip) */

/*  TL_TAB_LEN is calculated as:
*   13 - sinus amplitude bits     (Y axis)
*   2  - sinus sign bit           (Y axis)
*   TL_RES_LEN - sinus resolution (X axis)
*/
//#define TL_TAB_LEN (13*2*TL_RES_LEN)
#define TL_TAB_LEN (13*TL_RES_LEN*256/8) // 106496*2
UINT16 *ym_tl_tab;

/* ~3K wasted but oh well */
UINT16 ym_tl_tab2[13*TL_RES_LEN];
const UINT16 ym_quiet_row[256] DTCM_DATA = {0};

#define ENV_QUIET		(2*13*TL_RES_LEN/8)

/* sin waveform table in 'decibel' scale (use only period/4 values) */
static UINT16 ym_sin_tab[256];

static int ym_init_tab;

/* sustain level table (3dB per step) */
/* bit0, bit1, bit2, bit3, bit4, bit5, bit6 */
/* 1,    2,    4,    8,    16,   32,   64   (value)*/
/* 0.75, 1.5,  3,    6,    12,   24,   48   (dB)*/

/* 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)*/
#define SC(db) (UINT32) ( db * (4.0/ENV_STEP) )
static const UINT32 sl_table[16]={
 SC( 0),SC( 1),SC( 2),SC(3 ),SC(4 ),SC(5 ),SC(6 ),SC( 7),
 SC( 8),SC( 9),SC(10),SC(11),SC(12),SC(13),SC(14),SC(31)
};
#undef SC


#if 0
#define RATE_STEPS (8)
static const UINT8 eg_inc[19*RATE_STEPS]={

/*cycle:0 1  2 3  4 5  6 7*/

/* 0 */ 0,1, 0,1, 0,1, 0,1, /* rates 00..11 0 (increment by 0 or 1) */
/* 1 */ 0,1, 0,1, 1,1, 0,1, /* rates 00..11 1 */
/* 2 */ 0,1, 1,1, 0,1, 1,1, /* rates 00..11 2 */
/* 3 */ 0,1, 1,1, 1,1, 1,1, /* rates 00..11 3 */

/* 4 */ 1,1, 1,1, 1,1, 1,1, /* rate 12 0 (increment by 1) */
/* 5 */ 1,1, 1,2, 1,1, 1,2, /* rate 12 1 */
/* 6 */ 1,2, 1,2, 1,2, 1,2, /* rate 12 2 */
/* 7 */ 1,2, 2,2, 1,2, 2,2, /* rate 12 3 */

/* 8 */ 2,2, 2,2, 2,2, 2,2, /* rate 13 0 (increment by 2) */
/* 9 */ 2,2, 2,4, 2,2, 2,4, /* rate 13 1 */
/*10 */ 2,4, 2,4, 2,4, 2,4, /* rate 13 2 */
/*11 */ 2,4, 4,4, 2,4, 4,4, /* rate 13 3 */

/*12 */ 4,4, 4,4, 4,4, 4,4, /* rate 14 0 (increment by 4) */
/*13 */ 4,4, 4,8, 4,4, 4,8, /* rate 14 1 */
/*14 */ 4,8, 4,8, 4,8, 4,8, /* rate 14 2 */
/*15 */ 4,8, 8,8, 4,8, 8,8, /* rate 14 3 */

/*16 */ 8,8, 8,8, 8,8, 8,8, /* rates 15 0, 15 1, 15 2, 15 3 (increment by 8) */
/*17 */ 16,16,16,16,16,16,16,16, /* rates 15 2, 15 3 for attack */
/*18 */ 0,0, 0,0, 0,0, 0,0, /* infinity rates for attack and decay(s) */
};
#endif


#define PACK(a0,a1,a2,a3,a4,a5,a6,a7) ((a7<<21)|(a6<<18)|(a5<<15)|(a4<<12)|(a3<<9)|(a2<<6)|(a1<<3)|(a0<<0))
static const UINT32 eg_inc_pack[19] =
{
/* 0 */ PACK(0,1,0,1,0,1,0,1), /* rates 00..11 0 (increment by 0 or 1) */
/* 1 */ PACK(0,1,0,1,1,1,0,1), /* rates 00..11 1 */
/* 2 */ PACK(0,1,1,1,0,1,1,1), /* rates 00..11 2 */
/* 3 */ PACK(0,1,1,1,1,1,1,1), /* rates 00..11 3 */

/* 4 */ PACK(1,1,1,1,1,1,1,1), /* rate 12 0 (increment by 1) */
/* 5 */ PACK(1,1,1,2,1,1,1,2), /* rate 12 1 */
/* 6 */ PACK(1,2,1,2,1,2,1,2), /* rate 12 2 */
/* 7 */ PACK(1,2,2,2,1,2,2,2), /* rate 12 3 */

/* 8 */ PACK(2,2,2,2,2,2,2,2), /* rate 13 0 (increment by 2) */
/* 9 */ PACK(2,2,2,3,2,2,2,3), /* rate 13 1 */
/*10 */ PACK(2,3,2,3,2,3,2,3), /* rate 13 2 */
/*11 */ PACK(2,3,3,3,2,3,3,3), /* rate 13 3 */

/*12 */ PACK(3,3,3,3,3,3,3,3), /* rate 14 0 (increment by 4) */
/*13 */ PACK(3,3,3,4,3,3,3,4), /* rate 14 1 */
/*14 */ PACK(3,4,3,4,3,4,3,4), /* rate 14 2 */
/*15 */ PACK(3,4,4,4,3,4,4,4), /* rate 14 3 */

/*16 */ PACK(4,4,4,4,4,4,4,4), /* rates 15 0, 15 1, 15 2, 15 3 (increment by 8) */
/*17 */ PACK(5,5,5,5,5,5,5,5), /* rates 15 2, 15 3 for attack */
/*18 */ PACK(0,0,0,0,0,0,0,0), /* infinity rates for attack and decay(s) */
};


//#define O(a) (a*RATE_STEPS)
#define O(a) a

/*note that there is no O(17) in this table - it's directly in the code */
static const UINT8 eg_rate_select[32+64+32]={	/* Envelope Generator rates (32 + 64 rates + 32 RKS) */
/* 32 infinite time rates */
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),

/* rates 00-11 */
O(18),O(18),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),

/* rate 12 */
O( 4),O( 5),O( 6),O( 7),

/* rate 13 */
O( 8),O( 9),O(10),O(11),

/* rate 14 */
O(12),O(13),O(14),O(15),

/* rate 15 */
O(16),O(16),O(16),O(16),

/* 32 dummy rates (same as 15 3) */
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16)

};
#undef O

/*rate  0,    1,    2,   3,   4,   5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15*/
/*shift 11,   10,   9,   8,   7,   6,  5,  4,  3,  2, 1,  0,  0,  0,  0,  0 */
/*mask  2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3, 1,  0,  0,  0,  0,  0 */

#define O(a) (a*1)
static const UINT8 eg_rate_shift[32+64+32]={	/* Envelope Generator counter shifts (32 + 64 rates + 32 RKS) */
/* 32 infinite time rates */
O(11),O(11),O(11),O(11),O(11),O(11),O(11),O(11),
O(11),O(11),O(11),O(11),O(11),O(11),O(11),O(11),
O(11),O(11),O(11),O(11),O(11),O(11),O(11),O(11),
O(11),O(11),O(11),O(11),O(11),O(11),O(11),O(11),

/* rates 00-11 */
O(11),O(11),O(11),O(11),
O(10),O(10),O(10),O(10),
O( 9),O( 9),O( 9),O( 9),
O( 8),O( 8),O( 8),O( 8),
O( 7),O( 7),O( 7),O( 7),
O( 6),O( 6),O( 6),O( 6),
O( 5),O( 5),O( 5),O( 5),
O( 4),O( 4),O( 4),O( 4),
O( 3),O( 3),O( 3),O( 3),
O( 2),O( 2),O( 2),O( 2),
O( 1),O( 1),O( 1),O( 1),
O( 0),O( 0),O( 0),O( 0),

/* rate 12 */
O( 0),O( 0),O( 0),O( 0),

/* rate 13 */
O( 0),O( 0),O( 0),O( 0),

/* rate 14 */
O( 0),O( 0),O( 0),O( 0),

/* rate 15 */
O( 0),O( 0),O( 0),O( 0),

/* 32 dummy rates (same as 15 3) */
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0)

};
#undef O

static const UINT8 dt_tab[4 * 32]={
/* this is YM2151 and YM2612 phase increment data (in 10.10 fixed point format)*/
/* FD=0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* FD=1 */
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
	2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8,
/* FD=2 */
	1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5,
	5, 6, 6, 7, 8, 8, 9,10,11,12,13,14,16,16,16,16,
/* FD=3 */
	2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7,
	8 ,8, 9,10,11,12,13,14,16,17,19,20,22,22,22,22
};


/* OPN key frequency number -> key code follow table */
/* fnum higher 4bit -> keycode lower 2bit */
static const UINT8 opn_fktable[16] = {0,0,0,0,0,0,0,1,2,3,3,3,3,3,3,3};


/* 8 LFO speed parameters */
/* each value represents number of samples that one LFO level will last for */
static const UINT32 lfo_samples_per_step[8] = {108, 77, 71, 67, 62, 44, 8, 5};



/*There are 4 different LFO AM depths available, they are:
  0 dB, 1.4 dB, 5.9 dB, 11.8 dB
  Here is how it is generated (in EG steps):

  11.8 dB = 0, 2, 4, 6, 8, 10,12,14,16...126,126,124,122,120,118,....4,2,0
   5.9 dB = 0, 1, 2, 3, 4, 5, 6, 7, 8....63, 63, 62, 61, 60, 59,.....2,1,0
   1.4 dB = 0, 0, 0, 0, 1, 1, 1, 1, 2,...15, 15, 15, 15, 14, 14,.....0,0,0

  (1.4 dB is loosing precision as you can see)

  It's implemented as generator from 0..126 with step 2 then a shift
  right N times, where N is:
    8 for 0 dB
    3 for 1.4 dB
    1 for 5.9 dB
    0 for 11.8 dB
*/
static const UINT8 lfo_ams_depth_shift[4] = {8, 3, 1, 0};



/*There are 8 different LFO PM depths available, they are:
  0, 3.4, 6.7, 10, 14, 20, 40, 80 (cents)

  Modulation level at each depth depends on F-NUMBER bits: 4,5,6,7,8,9,10
  (bits 8,9,10 = FNUM MSB from OCT/FNUM register)

  Here we store only first quarter (positive one) of full waveform.
  Full table (lfo_pm_table) containing all 128 waveforms is build
  at run (init) time.

  One value in table below represents 4 (four) basic LFO steps
  (1 PM step = 4 AM steps).

  For example:
   at LFO SPEED=0 (which is 108 samples per basic LFO step)
   one value from "lfo_pm_output" table lasts for 432 consecutive
   samples (4*108=432) and one full LFO waveform cycle lasts for 13824
   samples (32*432=13824; 32 because we store only a quarter of whole
            waveform in the table below)
*/
static const UINT8 lfo_pm_output[7*8][8]={ /* 7 bits meaningful (of F-NUMBER), 8 LFO output levels per one depth (out of 32), 8 LFO depths */
/* FNUM BIT 4: 000 0001xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 2 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 3 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 4 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 5 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 6 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 7 */ {0,   0,   0,   0,   1,   1,   1,   1},

/* FNUM BIT 5: 000 0010xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 2 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 3 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 4 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 5 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 6 */ {0,   0,   0,   0,   1,   1,   1,   1},
/* DEPTH 7 */ {0,   0,   1,   1,   2,   2,   2,   3},

/* FNUM BIT 6: 000 0100xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 2 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 3 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 4 */ {0,   0,   0,   0,   0,   0,   0,   1},
/* DEPTH 5 */ {0,   0,   0,   0,   1,   1,   1,   1},
/* DEPTH 6 */ {0,   0,   1,   1,   2,   2,   2,   3},
/* DEPTH 7 */ {0,   0,   2,   3,   4,   4,   5,   6},

/* FNUM BIT 7: 000 1000xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 2 */ {0,   0,   0,   0,   0,   0,   1,   1},
/* DEPTH 3 */ {0,   0,   0,   0,   1,   1,   1,   1},
/* DEPTH 4 */ {0,   0,   0,   1,   1,   1,   1,   2},
/* DEPTH 5 */ {0,   0,   1,   1,   2,   2,   2,   3},
/* DEPTH 6 */ {0,   0,   2,   3,   4,   4,   5,   6},
/* DEPTH 7 */ {0,   0,   4,   6,   8,   8, 0xa, 0xc},

/* FNUM BIT 8: 001 0000xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   1,   1,   1,   1},
/* DEPTH 2 */ {0,   0,   0,   1,   1,   1,   2,   2},
/* DEPTH 3 */ {0,   0,   1,   1,   2,   2,   3,   3},
/* DEPTH 4 */ {0,   0,   1,   2,   2,   2,   3,   4},
/* DEPTH 5 */ {0,   0,   2,   3,   4,   4,   5,   6},
/* DEPTH 6 */ {0,   0,   4,   6,   8,   8, 0xa, 0xc},
/* DEPTH 7 */ {0,   0,   8, 0xc,0x10,0x10,0x14,0x18},

/* FNUM BIT 9: 010 0000xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   2,   2,   2,   2},
/* DEPTH 2 */ {0,   0,   0,   2,   2,   2,   4,   4},
/* DEPTH 3 */ {0,   0,   2,   2,   4,   4,   6,   6},
/* DEPTH 4 */ {0,   0,   2,   4,   4,   4,   6,   8},
/* DEPTH 5 */ {0,   0,   4,   6,   8,   8, 0xa, 0xc},
/* DEPTH 6 */ {0,   0,   8, 0xc,0x10,0x10,0x14,0x18},
/* DEPTH 7 */ {0,   0,0x10,0x18,0x20,0x20,0x28,0x30},

/* FNUM BIT10: 100 0000xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   4,   4,   4,   4},
/* DEPTH 2 */ {0,   0,   0,   4,   4,   4,   8,   8},
/* DEPTH 3 */ {0,   0,   4,   4,   8,   8, 0xc, 0xc},
/* DEPTH 4 */ {0,   0,   4,   8,   8,   8, 0xc,0x10},
/* DEPTH 5 */ {0,   0,   8, 0xc,0x10,0x10,0x14,0x18},
/* DEPTH 6 */ {0,   0,0x10,0x18,0x20,0x20,0x28,0x30},
/* DEPTH 7 */ {0,   0,0x20,0x30,0x40,0x40,0x50,0x60},

};

/* all 128 LFO PM waveforms */
static UINT8 *lfo_pm_table; /* 128 combinations of 7 bits meaningful (of F-NUMBER), 8 LFO depths, 32 LFO output levels per one depth */

/* there are 2048 FNUMs that can be generated using FNUM/BLK registers
	but LFO works with one more bit of a precision so we really need 4096 elements */
static UINT32 fn_table[4096];	/* fnumber->increment counter */

/* register number to channel number , slot offset */
#define OPN_CHAN(N) (N&3)
#define OPN_SLOT(N) ((N>>2)&3)

/* slot number */
#define SLOT1 0
#define SLOT2 2
#define SLOT3 1
#define SLOT4 3


/* OPN Mode Register Write */
static INLINE void set_timers( int v )
{
	/* b7 = CSM MODE */
	/* b6 = 3 slot mode */
	/* b5 = reset b */
	/* b4 = reset a */
	/* b3 = timer enable b */
	/* b2 = timer enable a */
	/* b1 = load b */
	/* b0 = load a */
	ym2612.OPN.ST.mode = v;

	/* reset Timer b flag */
	if( v & 0x20 )
		ym2612.OPN.ST.status &= ~2;

	/* reset Timer a flag */
	if( v & 0x10 )
		ym2612.OPN.ST.status &= ~1;
}

static INLINE void recalc_volout(FM_SLOT *SLOT)
{
	INT16 vol_out = SLOT->volume;
	if ((SLOT->ssg&0x0c) == 0x0c)
		vol_out = (0x200 - vol_out) & MAX_ATT_INDEX;
	SLOT->vol_out = vol_out + SLOT->tl;
}

static INLINE void FM_KEYON(int c , int s )
{
	FM_SLOT *SLOT = &ym2612.CH[c].SLOT[s];
	if( !SLOT->key )
	{
		SLOT->key = 1;
		SLOT->phase = 0;		/* restart Phase Generator */
		SLOT->ssg ^= SLOT->ssgn;
		SLOT->ssgn = 0;
		SLOT->state = (SLOT->sl == MIN_ATT_INDEX) ? EG_SUS : EG_DEC;
		if (SLOT->ar_ksr < 32+62) {
			if (SLOT->volume > MIN_ATT_INDEX) SLOT->state = EG_ATT;
		} else {
			SLOT->volume = MIN_ATT_INDEX;
		}
		recalc_volout(SLOT);
		ym2612.slot_mask |= (1<<s) << (c*4);
	}
}

static INLINE void FM_KEYOFF(int c , int s )
{
	FM_SLOT *SLOT = &ym2612.CH[c].SLOT[s];
	if( SLOT->key )
	{
		SLOT->key = 0;
		if (SLOT->state>EG_REL) {
			SLOT->state = EG_REL;/* phase -> Release */
			if (SLOT->ssg&0x08) {
				if (SLOT->ssg&0x04)
					SLOT->volume = (0x200 - SLOT->volume);
				if (SLOT->volume >= 0x200) {
					SLOT->volume = MAX_ATT_INDEX;
					SLOT->state  = EG_OFF;
				}
			}
		}
		SLOT->vol_out = SLOT->volume + SLOT->tl;
	}
}


/* set detune & multiple */
static INLINE void set_det_mul(FM_CH *CH, FM_SLOT *SLOT, int v)
{
	SLOT->mul = (v&0x0f)? (v&0x0f)*2 : 1;
	SLOT->DT  = ym2612.OPN.ST.dt_tab[(v>>4)&7];
	CH->SLOT[SLOT1].Incr=-1;
}

/* set total level */
static INLINE void set_tl(FM_SLOT *SLOT, int v)
{
	SLOT->tl = (v&0x7f)<<(ENV_BITS-7); /* 7bit TL */
	if (SLOT->state > EG_REL)
		recalc_volout(SLOT);
}

/* set attack rate & key scale  */
static INLINE void set_ar_ksr(FM_CH *CH, FM_SLOT *SLOT, int v)
{
	UINT8 old_KSR = SLOT->KSR;
	int eg_sh_ar, eg_sel_ar;

	SLOT->ar = (v&0x1f) ? 32 + ((v&0x1f)<<1) : 0;
	SLOT->ar_ksr = SLOT->ar + SLOT->ksr;

	SLOT->KSR = 3-(v>>6);
	if (SLOT->KSR != old_KSR)
	{
		CH->SLOT[SLOT1].Incr=-1;
	}

	/* refresh Attack rate */
	if ((SLOT->ar_ksr) < 32+62)
	{
		eg_sh_ar  = eg_rate_shift [SLOT->ar_ksr];
		eg_sel_ar = eg_rate_select[SLOT->ar_ksr];
	}
	else
	{
		eg_sh_ar  = 0;
		eg_sel_ar = 18;
	}

	SLOT->eg_pack_ar = eg_inc_pack[eg_sel_ar] | (eg_sh_ar<<24);
}

/* set decay rate */
static INLINE void set_dr(FM_SLOT *SLOT, int v)
{
	int eg_sh_d1r, eg_sel_d1r;

	SLOT->d1r = (v&0x1f) ? 32 + ((v&0x1f)<<1) : 0;

	eg_sh_d1r = eg_rate_shift [SLOT->d1r + SLOT->ksr];
	eg_sel_d1r= eg_rate_select[SLOT->d1r + SLOT->ksr];

	SLOT->eg_pack_d1r = eg_inc_pack[eg_sel_d1r] | (eg_sh_d1r<<24);
}

/* set sustain rate */
static INLINE void set_sr(FM_SLOT *SLOT, int v)
{
	int eg_sh_d2r, eg_sel_d2r;

	SLOT->d2r = (v&0x1f) ? 32 + ((v&0x1f)<<1) : 0;

	eg_sh_d2r = eg_rate_shift [SLOT->d2r + SLOT->ksr];
	eg_sel_d2r= eg_rate_select[SLOT->d2r + SLOT->ksr];

	SLOT->eg_pack_d2r = eg_inc_pack[eg_sel_d2r] | (eg_sh_d2r<<24);
}

/* set release rate */
static INLINE void set_sl_rr(FM_SLOT *SLOT, int v)
{
	int eg_sh_rr, eg_sel_rr;

	SLOT->sl = sl_table[ v>>4 ];

	if (SLOT->state == EG_DEC && (SLOT->volume >= (INT32)(SLOT->sl)))
		SLOT->state = EG_SUS;

	SLOT->rr  = 34 + ((v&0x0f)<<2);

	eg_sh_rr  = eg_rate_shift [SLOT->rr  + SLOT->ksr];
	eg_sel_rr = eg_rate_select[SLOT->rr  + SLOT->ksr];

	SLOT->eg_pack_rr = eg_inc_pack[eg_sel_rr] | (eg_sh_rr<<24);
}



static INLINE signed int op_calc(UINT32 phase, unsigned int env, signed int pm)
{
	int ret, sin = (phase>>16) + (pm>>1);
	int neg = sin & 0x200;
	if (sin & 0x100) sin ^= 0xff;
	sin&=0xff;
	env&=~1;

	// this was already checked
	// if (env >= ENV_QUIET) // 384
	//	return 0;

	ret = ym_tl_tab[sin | (env<<7)];

	return neg ? -ret : ret;
}

static INLINE signed int op_calc1(UINT32 phase, unsigned int env, signed int pm)
{
	int ret, sin = (phase+pm)>>16;
	int neg = sin & 0x200;
	if (sin & 0x100) sin ^= 0xff;
	sin&=0xff;
	env&=~1;

	// if (env >= ENV_QUIET) // 384
	//	return 0;

	ret = ym_tl_tab[sin | (env<<7)];

	return neg ? -ret : ret;
}

#if !defined(_ASM_YM2612_C) || defined(EXTERNAL_YM2612)
/* advance LFO to next sample */
static INLINE int advance_lfo(int lfo_ampm, UINT32 lfo_cnt_old, UINT32 lfo_cnt)
{
	UINT8 pos;
	UINT8 prev_pos;

	prev_pos = (lfo_cnt_old >> LFO_SH) & 127;

	pos = (lfo_cnt >> LFO_SH) & 127;

	/* update AM when LFO output changes */

	if (prev_pos != pos)
	{
		lfo_ampm &= 0xff;
		/* triangle (inverted) */
		/* AM: 0 to 126 step +2, 126 to 0 step -2 */
		if (pos<64)
			lfo_ampm |= ((pos^63) * 2) << 8;           /* 0 - 126 */
		else
			lfo_ampm |= ((pos&63) * 2) << 8;
	}
	else
	{
		return lfo_ampm;
	}

	/* PM works with 4 times slower clock */
	prev_pos >>= 2;
	pos >>= 2;
	/* update PM when LFO output changes */
	if (prev_pos != pos)
	{
		lfo_ampm &= ~0xff;
		lfo_ampm |= pos; /* 0 - 32 */
	}
	return lfo_ampm;
}

static INLINE void update_eg_phase(FM_SLOT *SLOT, UINT32 eg_cnt, UINT32 ssg_en)
{
	INT32 volume = SLOT->volume;
	UINT32 pack = SLOT->eg_pack[SLOT->state - 1];
	UINT32 shift = pack >> 24;
	INT32 eg_inc_val;

	if (eg_cnt & ((1 << shift) - 1))
		return;

	eg_inc_val = pack >> ((eg_cnt >> shift) & 7) * 3;
	eg_inc_val = (1 << (eg_inc_val & 7)) >> 1;

	if ((SLOT->ssg&0x08) && ssg_en) {
		switch (SLOT->state)
		{
		case EG_ATT:	/* attack phase */
			volume += ( ~volume * eg_inc_val ) >> 4;
			if ( volume <= MIN_ATT_INDEX )
			{
				volume = MIN_ATT_INDEX;
				SLOT->state = (SLOT->sl == MIN_ATT_INDEX) ? EG_SUS: EG_DEC;
			}
			break;

		case EG_DEC:	/* decay phase */
			if (volume < 0x200)
				volume += 4*eg_inc_val;
			if ( volume >= (INT32) SLOT->sl )
				SLOT->state = EG_SUS;
			break;

		case EG_SUS:	/* sustain phase */
			if (volume < 0x200)
				volume += 4*eg_inc_val;
			break;

		case EG_REL:	/* release phase */
			if (volume < 0x200)
				volume += 4*eg_inc_val;
			if ( volume >= 0x200 )
			{
				volume = MAX_ATT_INDEX;
				SLOT->state = EG_OFF;
			}
			break;
		}

		SLOT->vol_out = volume + SLOT->tl;
		if ((SLOT->ssg&0x04) && (SLOT->state > EG_REL))
			SLOT->vol_out = ((0x200 - volume) & MAX_ATT_INDEX) + SLOT->tl;
	} else {
		switch (SLOT->state)
		{
		case EG_ATT:		/* attack phase */
			volume += ( ~volume * eg_inc_val ) >> 4;
			if ( volume <= MIN_ATT_INDEX )
			{
				volume = MIN_ATT_INDEX;
				SLOT->state = (SLOT->sl == MIN_ATT_INDEX) ? EG_SUS: EG_DEC;
			}
			break;

		case EG_DEC:	/* decay phase */
			volume += eg_inc_val;
			if ( volume >= (INT32) SLOT->sl )
				SLOT->state = EG_SUS;
			break;

		case EG_SUS:	/* sustain phase */
			volume += eg_inc_val;
			if ( volume >= MAX_ATT_INDEX )
			{
				volume = MAX_ATT_INDEX;
				/* do not change SLOT->state (verified on real chip) */
			}
			break;

		case EG_REL:	/* release phase */
			volume += eg_inc_val;
			if ( volume >= MAX_ATT_INDEX )
			{
				volume = MAX_ATT_INDEX;
				SLOT->state = EG_OFF;
			}
			break;
		}

		SLOT->vol_out = volume + SLOT->tl;
	}
	SLOT->volume = volume;
}

static INLINE UINT32 update_ssg_eg_phase(FM_SLOT *SLOT, UINT32 phase)
{
	if (SLOT->ssg&0x01) {
		if (SLOT->ssg&0x02) {
			SLOT->ssg ^= SLOT->ssgn ^ 4;
			SLOT->ssgn = 4;
		}

		if (SLOT->state != EG_ATT && !(SLOT->ssg&0x04))
			SLOT->volume  = MAX_ATT_INDEX;
	} else {
		if (SLOT->ssg&0x02) {
			SLOT->ssg ^= 4;
			SLOT->ssgn ^= 4;
		} else
			phase = 0;

		if (SLOT->state != EG_ATT) {
			SLOT->state = (SLOT->sl == MIN_ATT_INDEX) ? EG_SUS : EG_DEC;
			if (SLOT->ar_ksr < 32+62) {
				if (SLOT->volume > MIN_ATT_INDEX) SLOT->state = EG_ATT;
			} else {
				SLOT->volume = MIN_ATT_INDEX;
			}
		}
	}
	recalc_volout(SLOT);
	return phase;
}
#endif





static unsigned fm_worker_channel=6;
#define FM_WORKER_CHANNEL fm_worker_channel
#include "fm_parallel.h"

#if !defined(_ASM_YM2612_C) || defined(EXTERNAL_YM2612)
#include <limits.h>
static int clip(int n) 
{
	unsigned b = 14, s = n < 0;
	int m = s + INT_MAX;
	if (s + (n>>(b-1))) n = m >> (8*sizeof(int)-b);
	return n;
}

static void update_ssg_eg_channel(chan_rend_context *ct)
{
	FM_SLOT *SLOT;

	SLOT = &ct->CH->SLOT[SLOT1];
	if ((SLOT->ssg&0x08) && SLOT->state > EG_REL && SLOT->volume >= 0x200)
		ct->phase1 = update_ssg_eg_phase(SLOT, ct->phase1);
	SLOT = &ct->CH->SLOT[SLOT2];
	if ((SLOT->ssg&0x08) && SLOT->state > EG_REL && SLOT->volume >= 0x200)
		ct->phase2 = update_ssg_eg_phase(SLOT, ct->phase2);
	SLOT = &ct->CH->SLOT[SLOT3];
	if ((SLOT->ssg&0x08) && SLOT->state > EG_REL && SLOT->volume >= 0x200)
		ct->phase3 = update_ssg_eg_phase(SLOT, ct->phase3);
	SLOT = &ct->CH->SLOT[SLOT4];
	if ((SLOT->ssg&0x08) && SLOT->state > EG_REL && SLOT->volume >= 0x200)
		ct->phase4 = update_ssg_eg_phase(SLOT, ct->phase4);
}

static void update_eg_phase_channel(chan_rend_context *ct)
{
	FM_SLOT *SLOT;

	SLOT = &ct->CH->SLOT[SLOT1];
	if (SLOT->state != EG_OFF) update_eg_phase(SLOT, ct->eg_cnt, ct->pack & 2);
	SLOT = &ct->CH->SLOT[SLOT2];
	if (SLOT->state != EG_OFF) update_eg_phase(SLOT, ct->eg_cnt, ct->pack & 2);
	SLOT = &ct->CH->SLOT[SLOT3];
	if (SLOT->state != EG_OFF) update_eg_phase(SLOT, ct->eg_cnt, ct->pack & 2);
	SLOT = &ct->CH->SLOT[SLOT4];
	if (SLOT->state != EG_OFF) update_eg_phase(SLOT, ct->eg_cnt, ct->pack & 2);
}

static int update_algo_channel(chan_rend_context *ct, unsigned int eg_out, unsigned int eg_out2, unsigned int eg_out4)
{
	int m2,c1,c2=0;	/* Phase Modulation input for operators 2,3,4 */
	int smp = 0;

	switch( ct->algo&0x7 )
	{
		case 0:
		{
			/* M1---C1---MEM---M2---C2---OUT */
			m2 = ct->mem;
			c1 = ct->op1_out>>16;
			if( eg_out2 < ENV_QUIET ) {		/* SLOT 2 */
				ct->mem = op_calc(ct->phase2, eg_out2, c1);
			}
			else ct->mem = 0;

			if( eg_out  < ENV_QUIET ) {		/* SLOT 3 */
				c2  = op_calc(ct->phase3, eg_out,  m2);
			}
			if( eg_out4 < ENV_QUIET ) {		/* SLOT 4 */
				smp = op_calc(ct->phase4, eg_out4, c2);
			}
			break;
		}
		case 1:
		{
			/* M1------+-MEM---M2---C2---OUT */
			/*      C1-+                     */
			m2 = ct->mem;
			ct->mem = ct->op1_out>>16;
			if( eg_out2 < ENV_QUIET ) {		/* SLOT 2 */
				ct->mem+= op_calc(ct->phase2, eg_out2, 0);
			}

			if( eg_out  < ENV_QUIET ) {		/* SLOT 3 */
				c2  = op_calc(ct->phase3, eg_out,  m2);
			}
			if( eg_out4 < ENV_QUIET ) {		/* SLOT 4 */
				smp = op_calc(ct->phase4, eg_out4, c2);
			}
			break;
		}
		case 2:
		{
			/* M1-----------------+-C2---OUT */
			/*      C1---MEM---M2-+          */
			m2 = ct->mem;
			c2 = ct->op1_out>>16;
			if( eg_out2 < ENV_QUIET ) {		/* SLOT 2 */
				ct->mem = op_calc(ct->phase2, eg_out2, 0);
			}
			else ct->mem = 0;

			if( eg_out  < ENV_QUIET ) {		/* SLOT 3 */
				c2 += op_calc(ct->phase3, eg_out,  m2);
			}
			if( eg_out4 < ENV_QUIET ) {		/* SLOT 4 */
				smp = op_calc(ct->phase4, eg_out4, c2);
			}
			break;
		}
		case 3:
		{
			/* M1---C1---MEM------+-C2---OUT */
			/*                 M2-+          */
			c2 = ct->mem;
			c1 = ct->op1_out>>16;
			if( eg_out2 < ENV_QUIET ) {		/* SLOT 2 */
				ct->mem = op_calc(ct->phase2, eg_out2, c1);
			}
			else ct->mem = 0;

			if( eg_out  < ENV_QUIET ) {		/* SLOT 3 */
				c2 += op_calc(ct->phase3, eg_out,  0);
			}
			if( eg_out4 < ENV_QUIET ) {		/* SLOT 4 */
				smp = op_calc(ct->phase4, eg_out4, c2);
			}
			break;
		}
		case 4:
		{
			/* M1---C1-+-OUT */
			/* M2---C2-+     */
			/* MEM: not used */

			c1 = ct->op1_out>>16;
			if( eg_out  < ENV_QUIET ) {		/* SLOT 3 */
				c2  = op_calc(ct->phase3, eg_out,  0);
			}
			if( eg_out2 < ENV_QUIET ) {		/* SLOT 2 */
				smp = op_calc(ct->phase2, eg_out2, c1);
			}
			if( eg_out4 < ENV_QUIET ) {		/* SLOT 4 */
				smp+= op_calc(ct->phase4, eg_out4, c2);
			}
			break;
		}
		case 5:
		{
			/*    +----C1----+     */
			/* M1-+-MEM---M2-+-OUT */
			/*    +----C2----+     */
			m2 = ct->mem;
			ct->mem = c1 = c2 = ct->op1_out>>16;

			if( eg_out < ENV_QUIET ) {		/* SLOT 3 */
				smp = op_calc(ct->phase3, eg_out, m2);
			}
			if( eg_out2 < ENV_QUIET ) {		/* SLOT 2 */
				smp+= op_calc(ct->phase2, eg_out2, c1);
			}
			if( eg_out4 < ENV_QUIET ) {		/* SLOT 4 */
				smp+= op_calc(ct->phase4, eg_out4, c2);
			}
			break;
		}
		case 6:
		{
			/* M1---C1-+     */
			/*      M2-+-OUT */
			/*      C2-+     */
			/* MEM: not used */

			c1 = ct->op1_out>>16;
			if( eg_out < ENV_QUIET ) {		/* SLOT 3 */
				smp = op_calc(ct->phase3, eg_out,  0);
			}
			if( eg_out2 < ENV_QUIET ) {		/* SLOT 2 */
				smp+= op_calc(ct->phase2, eg_out2, c1);
			}
			if( eg_out4 < ENV_QUIET ) {		/* SLOT 4 */
				smp+= op_calc(ct->phase4, eg_out4, 0);
			}
			break;
		}
		case 7:
		{
			/* M1-+     */
			/* C1-+-OUT */
			/* M2-+     */
			/* C2-+     */
			/* MEM: not used*/

			smp = ct->op1_out>>16;
			if( eg_out < ENV_QUIET ) {		/* SLOT 3 */
				smp += op_calc(ct->phase3, eg_out,  0);
			}
			if( eg_out2 < ENV_QUIET ) {		/* SLOT 2 */
				smp += op_calc(ct->phase2, eg_out2, 0);
			}
			if( eg_out4 < ENV_QUIET ) {		/* SLOT 4 */
				smp += op_calc(ct->phase4, eg_out4, 0);
			}
			break;
		}
	}
	return smp;
}

static void chan_render_loop(chan_rend_context *ct, s32 *buffer, int length)
{
	int scounter;					/* sample counter */

	/* sample generating loop */
	for (scounter = 0; scounter < length; scounter++)
	{
		int smp = 0;		/* produced sample */
		unsigned int eg_out, eg_out2, eg_out4;

		ct->eg_timer += ct->eg_timer_add;

		while (ct->eg_timer >= 1<<EG_SH) {
			ct->eg_timer -= 1<<EG_SH;

			if (ct->pack & 2)
				update_ssg_eg_channel(ct);

			if (ct->algo & 0x30)
				ct->algo -= 0x10;
			if (!(ct->algo & 0x30)) {
				ct->algo |= 0x30;
				ct->eg_cnt++;
				if (ct->eg_cnt >= 4096) ct->eg_cnt = 1;

				update_eg_phase_channel(ct);
			}
		}

		ct->vol_out1 =  ct->CH->SLOT[SLOT1].vol_out;
		ct->vol_out2 =  ct->CH->SLOT[SLOT2].vol_out;
		ct->vol_out3 =  ct->CH->SLOT[SLOT3].vol_out;
		ct->vol_out4 =  ct->CH->SLOT[SLOT4].vol_out;

		if (ct->pack & 4) goto disabled; /* output disabled */

		if (ct->pack & 8) { /* LFO enabled ? (test Earthworm Jim in between demo 1 and 2) */
			ct->pack = (ct->pack&0xffff) | (advance_lfo(ct->pack >> 16, ct->lfo_cnt, ct->lfo_cnt + ct->lfo_inc) << 16);
			ct->lfo_cnt += ct->lfo_inc;
		}

		/* calculate channel sample */
		eg_out = ct->vol_out1;
		if ( (ct->pack & 8) && (ct->pack&(1<<(SLOT1+8))) )
			eg_out += ct->pack >> (((ct->pack&0xc0)>>6)+24);

		if( eg_out < ENV_QUIET )	/* SLOT 1 */
		{
			int out = 0;

			if (ct->pack&0xf000) out = ((ct->op1_out + (ct->op1_out<<16))>>16) << ((ct->pack&0xf000)>>12); /* op1_out0 + op1_out1 */
			ct->op1_out <<= 16;
			ct->op1_out |= (unsigned short)op_calc1(ct->phase1, eg_out, out);
		} else {
			ct->op1_out <<= 16; /* op1_out0 = op1_out1; op1_out1 = 0; */
		}

		eg_out  = ct->vol_out3; // volume_calc(&CH->SLOT[SLOT3]);
		eg_out2 = ct->vol_out2; // volume_calc(&CH->SLOT[SLOT2]);
		eg_out4 = ct->vol_out4; // volume_calc(&CH->SLOT[SLOT4]);

		if (ct->pack & 8) {
			unsigned int add = ct->pack >> (((ct->pack&0xc0)>>6)+24);
			if (ct->pack & (1<<(SLOT3+8))) eg_out  += add;
			if (ct->pack & (1<<(SLOT2+8))) eg_out2 += add;
			if (ct->pack & (1<<(SLOT4+8))) eg_out4 += add;
		}

		smp = update_algo_channel(ct, eg_out, eg_out2, eg_out4);
		/* done calculating channel sample */
disabled:
		/* update phase counters AFTER output calculations */
		ct->phase1 += ct->incr1;
		ct->phase2 += ct->incr2;
		ct->phase3 += ct->incr3;
		ct->phase4 += ct->incr4;

		/* mix sample to output buffer */
		if (smp) {
			smp = clip(smp); /* saturate to 14 bit */
			if (ct->algo & 0x80) {
				smp &= ~0x1f; /* drop bits (DAC has 9 bits) */
				smp -= (smp < 0 ? 7:0) << 5; /* discontinuity */
			}
			if (ct->pack & 1) { /* stereo */
				if (ct->pack & 0x20) /* L */ /* TODO: check correctness */
					buffer[scounter*2] += smp;
				if (ct->pack & 0x10) /* R */
					buffer[scounter*2+1] += smp;
			} else {
				buffer[scounter] += smp;
			}
			ct->algo |= 8;
		}
	}
}
#else
void chan_render_loop(chan_rend_context *ct, s32 *buffer, unsigned short length);
#endif


#include "fm_parallel.h"
#ifdef __NDS__
static fm_shared shared_memory __attribute__((aligned(32)));
static channel_job *fm_jobs;
volatile unsigned fm_worker_wait_cycles,fm_worker_jobs;
volatile unsigned fm_worker_submit_cycles,fm_worker_collect_cycles;
extern volatile unsigned fm_error;
bool fm_parallel_init(void){
    memset(&shared_memory,0,sizeof(shared_memory));
    memcpy(shared_memory.lookup,ym_sin_tab,sizeof(ym_sin_tab));
    memcpy(shared_memory.lookup+256,ym_tl_tab2,sizeof(ym_tl_tab2));
    DC_FlushRange(&shared_memory,sizeof(shared_memory));
    fm_shared *shared=memUncached(&shared_memory);
    fm_jobs=shared->jobs;
    fifoSendAddress(FIFO_USER_01,&shared_memory);
    for(unsigned tries=0;!shared->ready;tries++){
        if(tries>120)return false;
        swiWaitForVBlank();
    }
    return shared->ready==1;
}
#else
static channel_job fm_jobs[2];
#endif
static int fm_last_channel;
static chan_rend_context crct DTCM_DATA;
static const unsigned char *fm_output_demand;
void YM2612SetOutputDemand_(const unsigned char *demand){fm_output_demand=demand;}


static void chan_render_prep(void)
{
	crct.eg_timer_add = ym2612.OPN.eg_timer_add * 4;
	crct.lfo_init_sft16 = ym2612.OPN.lfo_ampm << 16;
	crct.lfo_inc = ym2612.OPN.lfo_inc * 4;
}

static void chan_render_finish(s32 *buffer, int length, int active_chans)
{
	ym2612.OPN.eg_cnt = crct.eg_cnt;
	ym2612.OPN.eg_timer = crct.eg_timer;
	ym2612.OPN.lfo_cnt += ym2612.OPN.lfo_inc * (length * 4);
}

static __attribute__((noinline)) UINT32 update_lfo_phase(const FM_SLOT *SLOT, UINT32 block_fnum)
{
	UINT32 fnum_lfo;
	INT32  lfo_fn_table_index_offset;
	UINT8  blk;
	UINT32 fn;
	int fc,fdt;

	fnum_lfo   = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
	unsigned lfo_step=(crct.pack>>16)&31;
    unsigned quarter_step=(lfo_step&7)^((lfo_step&8)?7:0);
    lfo_fn_table_index_offset=lfo_pm_table[(fnum_lfo+crct.CH->pms)/4+quarter_step];
    if(lfo_step&16)lfo_fn_table_index_offset=-lfo_fn_table_index_offset;
	if (lfo_fn_table_index_offset)	/* LFO phase modulation active */
	{
		block_fnum = block_fnum*2 + lfo_fn_table_index_offset;
		blk = (block_fnum&0x7000) >> 12;
		fn  = block_fnum & 0xfff;

		/* phase increment counter */
		fc = (fn_table[fn]>>(7-blk));

		fdt = fc + SLOT->DT[crct.CH->kcode];
		if (fdt < 0) fdt += fn_table[0x7ff*2] >> 2;

		return (fdt * SLOT->mul) >> 1;
	} else
		return SLOT->Incr;
}

static int channel_commit(chan_rend_context *ct,int c){
	ct->CH->op1_out = ct->op1_out;
	ct->CH->mem_value = ct->mem;
	if (ct->CH->SLOT[SLOT1].state | ct->CH->SLOT[SLOT2].state | ct->CH->SLOT[SLOT3].state | ct->CH->SLOT[SLOT4].state)
	{
		ct->CH->SLOT[SLOT1].phase = ct->phase1;
		ct->CH->SLOT[SLOT2].phase = ct->phase2;
		ct->CH->SLOT[SLOT3].phase = ct->phase3;
		ct->CH->SLOT[SLOT4].phase = ct->phase4;
	}
	else
		ym2612.slot_mask &= ~(0xf << (c*4));
	ct->CH->upd_cnt = (ct->algo >> 4) & 0x7;

	return (ct->algo & 8) >> 3; // had output
}

static ITCM_CODE int channel_collect(s32 *buffer,int length){
    int active=0;
    for(int job_index=0;job_index<1;job_index++){
        int c=FM_WORKER_CHANNEL;channel_job *job=&fm_jobs[job_index];if(!job->active)continue;

#ifdef __NDS__
        extern volatile unsigned display_frames;
        unsigned perf_worker=system_perf_enter(PERF_WORKER);
        unsigned wait_start=display_frames;
        while(job->active!=2){
            if(display_frames-wait_start>120u){system_perf_leave(perf_worker);fm_error=7;return 0;}
        }
        system_perf_leave(perf_worker);
        fm_worker_wait_cycles+=display_frames-wait_start;fm_worker_jobs++;
#else
        chan_render_loop(&job->context,job->output,length);
#endif

#ifdef __NDS__
        /* ARM7 has published completion. Payload is now immutable until the
         * next job; invalidate stale lines and read it through ARM9 cache.
         * ARM9 never writes this cached view after the initial flush. */
        DC_InvalidateRange(&shared_memory.jobs[job_index],sizeof(channel_job));
        const channel_job *completed=&shared_memory.jobs[job_index];
#else
        const channel_job *completed=job;
#endif
        ym2612.CH[c]=completed->channel;
        chan_rend_context finished=completed->context;finished.CH=&ym2612.CH[c];
        active|=channel_commit(&finished,c)<<c;
        for(int n=0;n<length*2;n++)buffer[n]+=completed->output[n];
        if(fm_last_channel==c)crct=finished;
        job->active=0;
    }
    return active;
}



volatile unsigned fm_quiet_span_jobs,fm_quiet_span_samples;
static unsigned span_has_update(unsigned counter,unsigned updates,unsigned shift){
    if(shift>=32 || counter>=4096 || updates>171)return 1;
    unsigned end=counter+updates;
    if(end<4096)return (end>>shift)!=(counter>>shift);
    return (4095u>>shift)!=(counter>>shift) || ((end-4095)>>shift)!=0;
}
#ifndef __NDS__
unsigned probe_span_has_update(unsigned counter,unsigned updates,unsigned shift){
    return span_has_update(counter,updates,shift);
}
#endif


volatile unsigned fm_row_cache_hits,fm_row_cache_misses;
#ifdef __NDS__
static UINT16 hot_row_data[16][256] DTCM_DATA __attribute__((aligned(32)));
UINT16 *const ym_compact_lut=&hot_row_data[13][0];
static unsigned fm_compact_lut_ready; /* 0 invalid,1 exact log,2 linear gain */
UINT16 *const ym_linear_gain_lut=&hot_row_data[14][0];
INT16 *const ym_linear_sine_lut=(INT16 *)&hot_row_data[10][0];
static UINT16 hot_row_spill[2][256] __attribute__((aligned(32)));
static UINT8 hot_row_map[ENV_QUIET/2] DTCM_DATA;
static UINT16 hot_row_keys[18] DTCM_DATA;
static unsigned hot_row_cursor DTCM_DATA;
static ITCM_CODE const UINT16 *get_hot_row(unsigned volume,unsigned *pinned){
    if(volume>=ENV_QUIET)return ym_quiet_row;
    unsigned key=volume>>1,index=hot_row_map[key];
    if(index){index--;fm_row_cache_hits++;}
    else{
        index=hot_row_cursor;
        while(*pinned&(1u<<index)){if(++index==18)index=0;}
        if(hot_row_keys[index])hot_row_map[hot_row_keys[index]-1]=0;
        if(index<=15 && (index>=13 || (fm_compact_lut_ready==2 && index>=10)))fm_compact_lut_ready=0;
        memcpy(index<16?hot_row_data[index]:hot_row_spill[index-16],ym_tl_tab+(key<<8),512);
        hot_row_keys[index]=key+1;hot_row_map[key]=index+1;
        hot_row_cursor=index==17?0:index+1;fm_row_cache_misses++;
    }
    *pinned|=1u<<index;
    return index<16?hot_row_data[index]:hot_row_spill[index-16];
}
#endif

volatile unsigned fm_stable_jobs,fm_stable_samples;
volatile unsigned fm_hot_jobs,fm_hot_samples;
volatile unsigned fm_packed_hot_jobs,fm_packed_hot_samples;
static void fm_prepare_linear_lut(void);
static ITCM_CODE __attribute__((noinline)) void prepare_stable_loop(chan_rend_context *ct,unsigned length){
    ct->stable_loop=0;
    if(!length || length>128 || (ct->pack&14) ||
       ct->eg_timer_add!=262144 || ct->eg_timer>=65536 || ct->eg_cnt>=4096 ||
       (ct->algo&0x40))return;
    unsigned first=(ct->algo>>4)&3;if(!first)first=1;
    unsigned updates=length*4<first?0:1+(length*4-first)/3;
    unsigned remaining=length*4<first?first-length*4:3-(length*4-first)%3;
    if(!ct->stable_envelopes){
        for(unsigned slot=0;slot<4;slot++){
            const FM_SLOT *s=&ct->CH->SLOT[slot];
            if(s->state==EG_OFF)continue;
            if(s->state>EG_ATT)return;
            unsigned pack=s->eg_pack[s->state-1];
            if(s->state==EG_SUS && !(pack&0xffffff))continue;
            if(span_has_update(ct->eg_cnt,updates,pack>>24))return;
        }
        fm_quiet_span_jobs++;fm_quiet_span_samples+=length;
    }
    ct->eg_cnt+=updates;if(ct->eg_cnt>=4096)ct->eg_cnt-=4095;
    ct->algo=(ct->algo&~0x30u)|(remaining<<4);
    ct->vol_out1=ct->CH->SLOT[0].vol_out;
    ct->vol_out2=ct->CH->SLOT[2].vol_out;
    ct->vol_out3=ct->CH->SLOT[1].vol_out;
    ct->vol_out4=ct->CH->SLOT[3].vol_out;
    ct->stable_loop=1;fm_stable_jobs++;fm_stable_samples+=length;
    { /* A quiet operator has an exact zero row; phases and feedback still advance. */
        ct->stable_rows[0]=ct->vol_out1>=ENV_QUIET?ym_quiet_row:ym_tl_tab+((ct->vol_out1&~1u)<<7);
        ct->stable_rows[1]=ct->vol_out2>=ENV_QUIET?ym_quiet_row:ym_tl_tab+((ct->vol_out2&~1u)<<7);
        ct->stable_rows[2]=ct->vol_out3>=ENV_QUIET?ym_quiet_row:ym_tl_tab+((ct->vol_out3&~1u)<<7);
        ct->stable_rows[3]=ct->vol_out4>=ENV_QUIET?ym_quiet_row:ym_tl_tab+((ct->vol_out4&~1u)<<7);
#ifdef __NDS__
        /* ARM7 uses its own compact table and must never read ARM9 DTCM. */
        if(!((ct->CH==&ym2612.CH[FM_WORKER_CHANNEL]) && length>32)){
            /* Rows13..15 hold the exact compact sine/level tables. Keep
             * them resident instead of evicting/rebuilding them between
             * changing-envelope and stable-envelope channels. */
            if(!ct->pad && (ct->pack&1) && (ct->pack&0x30)){
                if(fm_compact_lut_ready!=2)fm_prepare_linear_lut();
                const unsigned v[4]={ct->vol_out1,ct->vol_out2,ct->vol_out3,ct->vol_out4};
                for(unsigned i=0;i<4;i++)ct->stable_rows[i]=(const UINT16 *)(uintptr_t)ym_linear_gain_lut[v[i]>=ENV_QUIET?416:(v[i]>>1)];
            }else{
            unsigned pinned=7u<<13;
            ct->stable_rows[0]=get_hot_row(ct->vol_out1,&pinned);
            ct->stable_rows[1]=get_hot_row(ct->vol_out2,&pinned);
            ct->stable_rows[2]=get_hot_row(ct->vol_out3,&pinned);
            ct->stable_rows[3]=get_hot_row(ct->vol_out4,&pinned);
            }
        }
#endif
        ct->stable_loop=2;fm_hot_jobs++;fm_hot_samples+=length;
        if((ct->pack&0x31)==0x31){ct->stable_loop=3;fm_packed_hot_jobs++;fm_packed_hot_samples+=length;}
    }
}
#ifndef __NDS__
unsigned probe_stable_counter(unsigned count,unsigned eg,unsigned length){
    FM_CH channel={0};chan_rend_context context={0};
    context.CH=&channel;context.stable_envelopes=1;context.eg_timer_add=65536;
    context.eg_cnt=eg;context.algo=count<<4;
    prepare_stable_loop(&context,length);
    return context.stable_loop?((context.eg_cnt<<8)|((context.algo>>4)&7)):0xffffffffu;
}
#endif


/* All boundaries here are internal to one original native render call.
 * No register write, resampler partition, oscillator, feedback or MEM state
 * is skipped. The SSG/LFO paths keep their existing per-sample loop. */
volatile unsigned fm_subspan_calls,fm_subspan_samples;
#ifdef __NDS__
extern void chan_render_loop_generic_fallback(chan_rend_context *,s32 *,unsigned short);
/* The reduced arithmetic loops do not preserve strong-feedback voices.
 * Slot5 / Outer Heaven exhibits clipping even with a full output buffer.
 * Keep those channels on the canonical ARM loop, including envelope clocks;
 * low-feedback channels retain the fast path and ARM7 remains available. */
static INLINE int needs_exact_feedback(const chan_rend_context *ct){
    return (ct->pack&0xf000)>=0x8000;
}
#endif
static ITCM_CODE void render_envelope_spans(chan_rend_context *ct,s32 *buffer,unsigned length){
#ifdef __NDS__
    if(needs_exact_feedback(ct)){
        chan_render_loop_generic_fallback(ct,buffer,length);return;
    }
#endif
#ifndef __NDS__
    chan_render_loop(ct,buffer,length);return;
#endif
    if(ct->stable_loop || (ct->pack&14) || ct->eg_timer_add!=262144 ||
       ct->eg_timer>=65536 || ct->eg_cnt>=4096 || (ct->algo&0x40)){
        chan_render_loop(ct,buffer,length);return;
    }
    /* Stable/changing normal stereo use the same signed amplitude model.
     * Source-write-free native calls retain every oscillator/envelope tick. */
    if(!ct->pad && (ct->pack&1) && (ct->pack&0x30)){
        chan_render_loop(ct,buffer,length);return;
    }
    /* Rapid envelopes do not amortize the extra dispatch/table preparation. */
    for(unsigned i=0;i<4;i++){
        const FM_SLOT *slot=&ct->CH->SLOT[i];
        if(slot->state && slot->state<=EG_ATT){
            unsigned pack=slot->eg_pack[slot->state-1];
            if((pack&0xffffff) && (pack>>24)<3){chan_render_loop(ct,buffer,length);return;}
        }
    }
    unsigned active=0,original_demand=ct->pad;
    while(length){
        unsigned first=(ct->algo>>4)&3;if(!first)first=1;
        unsigned quiet=length;
        for(unsigned i=0;i<4;i++){
            const FM_SLOT *slot=&ct->CH->SLOT[i];
            if(!slot->state)continue;
            unsigned pack=slot->eg_pack[slot->state-1],shift=pack>>24;
            if(slot->state==EG_SUS && !(pack&0xffffff))continue;
            if(shift>=12)continue; /* Counter wraps4095->1, never reaches4096. */
            unsigned period=1u<<shift;
            unsigned distance=period-(ct->eg_cnt&(period-1));
            if(ct->eg_cnt+distance>=4096)distance=4095-ct->eg_cnt+period;
            unsigned before=(first+(distance-1)*3-1)/4;
            if(before<quiet)quiet=before;
        }
        unsigned take;
        if(quiet>=8){
            take=quiet;prepare_stable_loop(ct,take);
            assert(ct->stable_loop);
            fm_subspan_calls++;fm_subspan_samples+=take;
        }else{
            take=quiet<length?quiet+1:length;ct->stable_loop=0;
        }
        chan_render_loop(ct,buffer,take);active|=ct->algo&8;
        buffer+=take*2;length-=take;if(ct->pad)ct->pad+=take;
    }
    ct->algo|=active;ct->pad=original_demand;
}

static void fm_restore_linear_rows(chan_rend_context *ct){
    if(ct->pad || (ct->pack&14) || ct->eg_timer_add!=262144 || ct->eg_timer>=65536)return;
    for(unsigned i=0;i<4;i++){
        unsigned slot=i==1?2:i==2?1:i,v=ct->CH->SLOT[slot].vol_out;
        ct->stable_rows[i]=v>=ENV_QUIET?ym_quiet_row:ym_tl_tab+((v&~1u)<<7);
    }
}
extern void fm_observe_stack(void);
static ITCM_CODE int chan_render(s32 *buffer, int length, int c, UINT32 flags) // flags: stereo, ?, disabled, ?, pan_r, pan_l
{
	if(c>fm_last_channel)fm_last_channel=c;fm_observe_stack();
    crct.pad=(UINT32)(uintptr_t)fm_output_demand;
	crct.CH = &ym2612.CH[c];
	crct.mem = crct.CH->mem_value;		/* one sample delay memory */
	crct.lfo_cnt = ym2612.OPN.lfo_cnt;

	flags &= 0x37;

	if (crct.lfo_inc) {
		flags |= 8;
		flags |= crct.lfo_init_sft16;
		flags |= crct.CH->AMmasks << 8;
		if (crct.CH->ams == 8) // no ams
		     flags &= ~0xf00;
		else flags |= (crct.CH->ams&3)<<6;
	}
	flags |= (crct.CH->FB&0xf)<<12;				/* feedback shift */
	crct.pack = flags;

	crct.eg_cnt = ym2612.OPN.eg_cnt;			/* envelope generator counter */
	crct.eg_timer = ym2612.OPN.eg_timer;

	/* precalculate phase modulation incr */
	crct.phase1 = crct.CH->SLOT[SLOT1].phase;
	crct.phase2 = crct.CH->SLOT[SLOT2].phase;
	crct.phase3 = crct.CH->SLOT[SLOT3].phase;
	crct.phase4 = crct.CH->SLOT[SLOT4].phase;

	crct.op1_out = crct.CH->op1_out;
	crct.algo = crct.CH->ALGO & 7;
	crct.algo |= crct.CH->upd_cnt << 4;
	if (ym2612.OPN.ST.flags & ST_DAC)
		crct.algo |= 0x80;

	if(crct.CH->pms && (ym2612.OPN.ST.mode & 0xC0) && c == 2) {
		/* 3 slot mode */
		crct.incr1 = update_lfo_phase(&crct.CH->SLOT[SLOT1], ym2612.OPN.SL3.block_fnum[1]);
		crct.incr2 = update_lfo_phase(&crct.CH->SLOT[SLOT2], ym2612.OPN.SL3.block_fnum[2]);
		crct.incr3 = update_lfo_phase(&crct.CH->SLOT[SLOT3], ym2612.OPN.SL3.block_fnum[0]);
		crct.incr4 = update_lfo_phase(&crct.CH->SLOT[SLOT4], crct.CH->block_fnum);
	}
	else if(crct.CH->pms)
	{
		crct.incr1 = update_lfo_phase(&crct.CH->SLOT[SLOT1], crct.CH->block_fnum);
		crct.incr2 = update_lfo_phase(&crct.CH->SLOT[SLOT2], crct.CH->block_fnum);
		crct.incr3 = update_lfo_phase(&crct.CH->SLOT[SLOT3], crct.CH->block_fnum);
		crct.incr4 = update_lfo_phase(&crct.CH->SLOT[SLOT4], crct.CH->block_fnum);
	}
	else	/* no LFO phase modulation */
	{
		crct.incr1 = crct.CH->SLOT[SLOT1].Incr;
		crct.incr2 = crct.CH->SLOT[SLOT2].Incr;
		crct.incr3 = crct.CH->SLOT[SLOT3].Incr;
		crct.incr4 = crct.CH->SLOT[SLOT4].Incr;
	}


    crct.incr1*=4;crct.incr2*=4;crct.incr3*=4;crct.incr4*=4;
    crct.stable_envelopes=!(flags&2);
    for(unsigned slot=0;slot<4;slot++){
        FM_SLOT *s=&crct.CH->SLOT[slot];
        if(s->state!=EG_OFF && !(s->state==EG_SUS && !(s->eg_pack_d2r&0xffffff)))
            crct.stable_envelopes=0;
    }

#ifdef __NDS__
    /* prepare_stable_loop advances EG time: bypass it together with the
     * optimized renderer, so the canonical loop advances each tick once. */
    if(needs_exact_feedback(&crct))crct.stable_loop=0;
    else prepare_stable_loop(&crct,length);
#endif
    if(c==FM_WORKER_CHANNEL && length>32){
        channel_job *job=&fm_jobs[0];
        assert(length<=128);
        job->context=crct;job->channel=*crct.CH;
        job->context.CH=&job->channel;
#ifdef __NDS__
        if(fm_output_demand)memcpy(job->demand,fm_output_demand,length);
#endif
        
#ifndef __NDS__
        memset(job->output,0,length*2*sizeof(s32));
#endif
        job->length=length;job->active=1;
#ifdef __NDS__
        fifoSendValue32(FIFO_USER_02,0);
#endif
        return 0;
    }
    /* Constant envelopes use only the context/table rows. Copy only the
     * changing channel, whose per-operator state otherwise competes with
     * the tone table for the small ARM9 data cache. */
    if(!crct.stable_loop){
        FM_CH *original=crct.CH;
        audio_hot_state.fm=*original;
        crct.CH=&audio_hot_state.fm;
        render_envelope_spans(&crct,buffer,length);
    fm_restore_linear_rows(&crct);
        int active=channel_commit(&crct,c);
        *original=audio_hot_state.fm;
        crct.CH=original;
        return active;
    }
	render_envelope_spans(&crct,buffer,length);
    fm_restore_linear_rows(&crct);

	return channel_commit(&crct,c);
}

/* update phase increment and envelope generator */
static __attribute__((noinline)) void refresh_fc_eg_slot(FM_SLOT *SLOT, int fc, int kc)
{
	int ksr, fdt;

	/* (frequency) phase increment counter */
	fdt = fc+SLOT->DT[kc];
	/* detect overflow */
//	if (fdt < 0) fdt += fn_table[0x7ff*2] >> (7-blk-1);
	if (fdt < 0) fdt += fn_table[0x7ff*2] >> 2;
	SLOT->Incr = fdt*SLOT->mul >> 1;

	ksr = kc >> SLOT->KSR;
	if( SLOT->ksr != ksr )
	{
		int eg_sh, eg_sel;
		SLOT->ksr = ksr;
		SLOT->ar_ksr = SLOT->ar + ksr;

		/* calculate envelope generator rates */
		if ((SLOT->ar_ksr) < 32+62)
		{
			eg_sh  = eg_rate_shift [SLOT->ar_ksr];
			eg_sel = eg_rate_select[SLOT->ar_ksr];
		}
		else
		{
			eg_sh  = 0;
			eg_sel = 18;
		}

		SLOT->eg_pack_ar = eg_inc_pack[eg_sel] | (eg_sh<<24);

		eg_sh  = eg_rate_shift [SLOT->d1r + ksr];
		eg_sel = eg_rate_select[SLOT->d1r + ksr];

		SLOT->eg_pack_d1r = eg_inc_pack[eg_sel] | (eg_sh<<24);

		eg_sh  = eg_rate_shift [SLOT->d2r + ksr];
		eg_sel = eg_rate_select[SLOT->d2r + ksr];

		SLOT->eg_pack_d2r = eg_inc_pack[eg_sel] | (eg_sh<<24);

		eg_sh  = eg_rate_shift [SLOT->rr  + ksr];
		eg_sel = eg_rate_select[SLOT->rr  + ksr];

		SLOT->eg_pack_rr = eg_inc_pack[eg_sel] | (eg_sh<<24);
	}
}

/* update phase increment counters */
static __attribute__((noinline)) void refresh_fc_eg_chan(FM_CH *CH)
{
	if( CH->SLOT[SLOT1].Incr==-1){
		int fc = CH->fc;
		int kc = CH->kcode;
		refresh_fc_eg_slot(&CH->SLOT[SLOT1] , fc , kc );
		refresh_fc_eg_slot(&CH->SLOT[SLOT2] , fc , kc );
		refresh_fc_eg_slot(&CH->SLOT[SLOT3] , fc , kc );
		refresh_fc_eg_slot(&CH->SLOT[SLOT4] , fc , kc );
	}
}

static __attribute__((noinline)) void refresh_fc_eg_chan_sl3(void)
{
	if( ym2612.CH[2].SLOT[SLOT1].Incr==-1)
	{
		refresh_fc_eg_slot(&ym2612.CH[2].SLOT[SLOT1], ym2612.OPN.SL3.fc[1], ym2612.OPN.SL3.kcode[1] );
		refresh_fc_eg_slot(&ym2612.CH[2].SLOT[SLOT2], ym2612.OPN.SL3.fc[2], ym2612.OPN.SL3.kcode[2] );
		refresh_fc_eg_slot(&ym2612.CH[2].SLOT[SLOT3], ym2612.OPN.SL3.fc[0], ym2612.OPN.SL3.kcode[0] );
		refresh_fc_eg_slot(&ym2612.CH[2].SLOT[SLOT4], ym2612.CH[2].fc , ym2612.CH[2].kcode );
	}
}

/* initialize time tables */
static void init_timetables(const UINT8 *dttable)
{
	int i,d;
	double rate;

	/* DeTune table */
	for (d = 0;d <= 3;d++){
		for (i = 0;i <= 31;i++){
			rate = ((double)dttable[d*32 + i]) * ym2612.OPN.ST.freqbase  * (1<<(FREQ_SH-10));
			ym2612.OPN.ST.dt_tab[d][i]   = (INT32) rate;
			ym2612.OPN.ST.dt_tab[d+4][i] = -ym2612.OPN.ST.dt_tab[d][i];
		}
	}
}


static void reset_channels(FM_CH *CH)
{
	int c,s;

	ym2612.OPN.ST.mode   = 0;	/* normal mode */
	ym2612.OPN.ST.TA     = 0;
	//ym2612.OPN.ST.TAC    = 0;
	ym2612.OPN.ST.TB     = 0;
	//ym2612.OPN.ST.TBC    = 0;

	for( c = 0 ; c < 6 ; c++ )
	{
		CH[c].fc = 0;
		for(s = 0 ; s < 4 ; s++ )
		{
			CH[c].SLOT[s].Incr = -1;
			CH[c].SLOT[s].key = 0;
			CH[c].SLOT[s].phase = 0;
			CH[c].SLOT[s].ar = CH[c].SLOT[s].ksr = 0;
			CH[c].SLOT[s].ar_ksr = 0;
			CH[c].SLOT[s].ssg = CH[c].SLOT[s].ssgn = 0;
			CH[c].SLOT[s].state= EG_OFF;
			CH[c].SLOT[s].volume = MAX_ATT_INDEX;
			CH[c].SLOT[s].vol_out = MAX_ATT_INDEX;
		}
		CH[c].mem_value = CH[c].op1_out = 0;
	}
	ym2612.slot_mask = 0;
	ym2612.ssg_mask = 0;
}

/* initialize generic tables */
#include "ym_init_values_r22.inc"
#ifdef __NDS__
/* The stable loop pins its rows only until it returns. A changing-envelope
 * call owns this scratch table exclusively and invalidates all three old keys
 * before overwriting them. A later stable miss marks the table unavailable. */
static const UINT16 fm_half_sine[512]={
 2137,1731,1543,1419,1326,1252,1190,1137,1091,1050,1013,979,949,920,894,869,
 846,825,804,785,767,749,732,717,701,687,672,659,646,633,621,609,
 598,587,576,566,556,546,536,527,518,509,501,492,484,476,468,461,
 453,446,439,432,425,418,411,405,399,392,386,380,375,369,363,358,
 352,347,341,336,331,326,321,316,311,307,302,297,293,289,284,280,
 276,271,267,263,259,255,251,248,244,240,236,233,229,226,222,219,
 215,212,209,205,202,199,196,193,190,187,184,181,178,175,172,169,
 167,164,161,159,156,153,151,148,146,143,141,138,136,134,131,129,
 127,125,122,120,118,116,114,112,110,108,106,104,102,100,98,96,
 94,92,91,89,87,85,83,82,80,78,77,75,74,72,70,69,
 67,66,64,63,62,60,59,57,56,55,53,52,51,49,48,47,
 46,45,43,42,41,40,39,38,37,36,35,34,33,32,31,30,
 29,28,27,26,25,24,23,23,22,21,20,20,19,18,17,17,
 16,15,15,14,13,13,12,12,11,10,10,9,9,8,8,7,
 7,7,6,6,5,5,5,4,4,4,3,3,3,2,2,2,
 2,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,2,
 2,2,2,3,3,3,4,4,4,5,5,5,6,6,7,7,
 7,8,8,9,9,10,10,11,12,12,13,13,14,15,15,16,
 17,17,18,19,20,20,21,22,23,23,24,25,26,27,28,29,
 30,31,32,33,34,35,36,37,38,39,40,41,42,43,45,46,
 47,48,49,51,52,53,55,56,57,59,60,62,63,64,66,67,
 69,70,72,74,75,77,78,80,82,83,85,87,89,91,92,94,
 96,98,100,102,104,106,108,110,112,114,116,118,120,122,125,127,
 129,131,134,136,138,141,143,146,148,151,153,156,159,161,164,167,
 169,172,175,178,181,184,187,190,193,196,199,202,205,209,212,215,
 219,222,226,229,233,236,240,244,248,251,255,259,263,267,271,276,
 280,284,289,293,297,302,307,311,316,321,326,331,336,341,347,352,
 358,363,369,375,380,386,392,399,405,411,418,425,432,439,446,453,
 461,468,476,484,492,501,509,518,527,536,546,556,566,576,587,598,
 609,621,633,646,659,672,687,701,717,732,749,767,785,804,825,846,
 869,894,920,949,979,1013,1050,1091,1137,1190,1252,1326,1419,1543,1731,2137,
};
void fm_prepare_compact_lut(void){
    if(fm_compact_lut_ready==1)return;
    for(unsigned i=13;i<16;i++){
        if(hot_row_keys[i])hot_row_map[hot_row_keys[i]-1]=0;
        hot_row_keys[i]=0;
    }
    memcpy(ym_compact_lut,fm_half_sine,1024);
    memcpy(ym_compact_lut+512,ym_initial_level,512);
    fm_compact_lut_ready=1;
}
#include "fm_linear_gain.inc"
static void fm_prepare_linear_lut(void){
    if(fm_compact_lut_ready==2)return;
    /* Scratch rows are unpinned between synchronous render calls. Remove
     * stale cache owners before switching either table representation. */
    for(unsigned i=10;i<16;i++){
        if(hot_row_keys[i])hot_row_map[hot_row_keys[i]-1]=0;
        hot_row_keys[i]=0;
    }
    memcpy(ym_linear_sine_lut,fm_linear_sine,sizeof(fm_linear_sine));
    memcpy(ym_linear_gain_lut,fm_linear_gain,sizeof(fm_linear_gain));
    fm_compact_lut_ready=2;
}
/* BEGIN per-call envelope schedule. No source clocks or oscillator steps skip. */
unsigned fm_prepare_envelope_mask(chan_rend_context *ct,unsigned length){
    fm_prepare_compact_lut();
    if(ct->stable_envelopes)return 0;
    if(!length || length>128 || (ct->pack&14) || ct->eg_timer_add!=262144 ||
       ct->eg_timer>=65536 || ct->eg_cnt>=4096 || (ct->algo&0x40))return 15;
    unsigned first=(ct->algo>>4)&3;if(!first)first=1;
    unsigned updates=length*4<first?0:1+(length*4-first)/3,mask=0;
    for(unsigned op=0;op<4;op++){
        unsigned index=op==1?2:op==2?1:op;
        const FM_SLOT *slot=&ct->CH->SLOT[index];
        if(slot->state==EG_OFF)continue;
        if(slot->state>EG_ATT){mask|=1u<<op;continue;}
        unsigned pack=slot->eg_pack[slot->state-1];
        if(slot->state==EG_SUS && !(pack&0xffffff) && slot->volume>=0 && slot->volume<=1023 &&
           slot->vol_out==slot->volume+slot->tl)continue;
        if(span_has_update(ct->eg_cnt,updates,pack>>24))mask|=1u<<op;
    }
    return mask;
}
/* Transient stack records: packed rate, low12-bit period+state, row address.
 * The native3 fast loop advances counter4095->1, so only low12period bits
 * can participate in its test. No private descriptor is serialized. */
ITCM_CODE __attribute__((noinline,optimize("Os"))) unsigned fm_prepare_envelope_schedule(chan_rend_context *ct,unsigned length,unsigned *records){
    if(fm_compact_lut_ready!=2)fm_prepare_linear_lut();
    if(ct->stable_envelopes)return 0;
    if(!length || length>128 || (ct->pack&14) || ct->eg_timer_add!=262144 ||
       ct->eg_timer>=65536 || ct->eg_cnt>=4096 || (ct->algo&0x40))return ~0u;
    unsigned first=(ct->algo>>4)&3;if(!first)first=1;
    unsigned updates=length*4<first?0:1+(length*4-first)/3;
    unsigned mask=0,end=ct->eg_cnt+updates;bool fast=true;
    for(unsigned op=0;op<4;op++){
        unsigned index=op==1?2:op==2?1:op;
        const FM_SLOT *slot=&ct->CH->SLOT[index];
        unsigned packed=0,period=4095;
        if(slot->state){
            if(slot->state>4)return ~0u;
            packed=slot->eg_pack[slot->state-1];
            bool constant=slot->state==EG_SUS && !(packed&0xffffff) &&
                slot->volume>=0 && slot->volume<=1023 && slot->vol_out==slot->volume+slot->tl;
            unsigned shift=packed>>24;
            /* Exact original span predicate on this guarded native3 grid.
             * A four-tick native output can advance one or two original EG ticks. */
            bool due=shift>=32 || (end<4096?
                (end>>shift)!=(ct->eg_cnt>>shift):
                ((4095u>>shift)!=(ct->eg_cnt>>shift) || ((end-4095)>>shift)!=0));
            if(!constant && due){
                mask|=1u<<op;
                period=(shift>=12?4095:(1u<<shift)-1)|(slot->state<<16);
                if(period&4095)fast=false;
            }
        }
        records[op*3]=packed;records[op*3+1]=period;
        records[op*3+2]=(unsigned)(uintptr_t)&ct->stable_rows[op];
    }
    return mask?(mask|(fast?16:0)):0;
}
/* END per-call envelope schedule. */

#endif
static void init_tables(void)
{
    signed int i,x,y,p;
    if(ym_init_tab)return;
    ym_init_tab=1;
    memcpy(ym_sin_tab,ym_initial_sine,sizeof(ym_sin_tab));
#ifdef __NDS__
    fm_compact_lut_ready=false;fm_prepare_compact_lut();
#endif
    for(x=0;x<TL_RES_LEN;x++)
        for(i=0;i<13;i++)ym_tl_tab2[x+i*TL_RES_LEN]=ym_initial_level[x]>>i;

	for (x=0; x < 256; x++)
	{
		int sin = ym_sin_tab[ x ];

		for (y=0; y < 2*13*TL_RES_LEN/8; y+=2)
		{
			p = (y<<2) + sin;
			if (p >= 13*TL_RES_LEN)
				 ym_tl_tab[(y<<7) | x] = 0;
			else ym_tl_tab[(y<<7) | x] = ym_tl_tab2[p];
		}
	}


	/* build LFO PM modulation table */
	for(i = 0; i < 8; i++) /* 8 PM depths */
	{
		UINT8 fnum;
		for (fnum=0; fnum<128; fnum++) /* 7 bits meaningful of F-NUMBER */
		{
			UINT8 value;
			UINT8 step;
			UINT32 offset_depth = i;
			UINT32 offset_fnum_bit;
			UINT32 bit_tmp;

			for (step=0; step<8; step++)
			{
				value = 0;
				for (bit_tmp=0; bit_tmp<7; bit_tmp++) /* 7 bits */
				{
					if (fnum & (1<<bit_tmp)) /* only if bit "bit_tmp" is set */
					{
						offset_fnum_bit = bit_tmp * 8;
						value += lfo_pm_output[offset_fnum_bit + offset_depth][step];
					}
				}
				lfo_pm_table[(fnum*8*8)+(i*8)+step]=value;
			}
		}
	}
}


/* CSM Key Controll */
#if 0
INLINE void CSMKeyControll(FM_CH *CH)
{
	/* this is wrong, atm */

	/* all key on */
	FM_KEYON(CH,SLOT1);
	FM_KEYON(CH,SLOT2);
	FM_KEYON(CH,SLOT3);
	FM_KEYON(CH,SLOT4);
}
#endif


/* prescaler set (and make time tables) */
static void OPNSetPres(int pres)
{
	int i;

	/* frequency base */
	double freqbase = (ym2612.OPN.ST.rate) ? ((double)ym2612.OPN.ST.clock / ym2612.OPN.ST.rate) / pres : 0;

	ym2612.OPN.eg_timer_add  = (1<<EG_SH) * freqbase;
	ym2612.OPN.ST.freqbase = freqbase;

	/* make time tables */
	init_timetables( dt_tab );

	/* there are 2048 FNUMs that can be generated using FNUM/BLK registers
        but LFO works with one more bit of a precision so we really need 4096 elements */
	/* calculate fnumber -> increment counter table */
	for(i = 0; i < 4096; i++)
	{
		/* freq table for octave 7 */
		/* OPN phase increment counter = 20bit */
		fn_table[i] = (UINT32)( (double)i * 32 * ym2612.OPN.ST.freqbase * (1<<(FREQ_SH-10)) ); /* -10 because chip works with 10.10 fixed point, while we use 16.16 */
	}

	/* LFO freq. table */
	for(i = 0; i < 8; i++)
	{
		/* Amplitude modulation: 64 output levels (triangle waveform); 1 level lasts for one of "lfo_samples_per_step" samples */
		/* Phase modulation: one entry from lfo_pm_output lasts for one of 4 * "lfo_samples_per_step" samples  */
		ym2612.OPN.lfo_freq[i] = (1.0 / lfo_samples_per_step[i]) * (1<<LFO_SH) * ym2612.OPN.ST.freqbase;
	}
}


/* write a OPN register (0x30-0xff) */
static int OPNWriteReg(int r, int v)
{
	int ret = 1;
	FM_CH *CH;
	FM_SLOT *SLOT;

	UINT8 c = OPN_CHAN(r);

	if (c == 3) return 0; /* 0xX3,0xX7,0xXB,0xXF */

	if (r >= 0x100) c+=3;

	CH = &ym2612.CH[c];

	SLOT = &(CH->SLOT[OPN_SLOT(r)]);

	switch( r & 0xf0 ) {
	case 0x30:	/* DET , MUL */
		set_det_mul(CH,SLOT,v);
		break;

	case 0x40:	/* TL */
		set_tl(SLOT,v);
		break;

	case 0x50:	/* KS, AR */
		set_ar_ksr(CH,SLOT,v);
		break;

	case 0x60:	/* bit7 = AM ENABLE, DR | depends on ksr */
		set_dr(SLOT,v);
		if(v&0x80) CH->AMmasks |=   1<<OPN_SLOT(r);
		else       CH->AMmasks &= ~(1<<OPN_SLOT(r));
		break;

	case 0x70:	/*     SR | depends on ksr */
		set_sr(SLOT,v);
		break;

	case 0x80:	/* SL, RR | depends on ksr */
		set_sl_rr(SLOT,v);
		break;

	case 0x90:	/* SSG-EG */
		SLOT->ssg =  v&0x0f;
		SLOT->ssg ^= SLOT->ssgn;
		if (v&0x08) ym2612.ssg_mask |=   1<<(OPN_SLOT(r) + c*4);
		else        ym2612.ssg_mask &= ~(1<<(OPN_SLOT(r) + c*4));
		if (SLOT->state > EG_REL)
			recalc_volout(SLOT);
		break;

	case 0xa0:
		switch( OPN_SLOT(r) ){
		case 0:		/* 0xa0-0xa2 : FNUM1 | depends on fn_h (below) */
			{
				UINT32 fn = ((UINT32)(ym2612.OPN.ST.fn_h & 7) << 8) | v;
				UINT8 blk = ym2612.OPN.ST.fn_h >> 3;
				/* keyscale code */
				CH->kcode = (blk<<2) | opn_fktable[fn >> 7];
				/* phase increment counter */
				CH->fc = fn_table[fn*2]>>(7-blk);

				/* store fnum in clear form for LFO PM calculations */
				CH->block_fnum = (blk<<11) | fn;

				CH->SLOT[SLOT1].Incr=-1;
			}
			break;
		case 1:		/* 0xa4-0xa6 : FNUM2,BLK */
			ym2612.OPN.ST.fn_h = v & 0x3f;
			ret = 0;
			break;
		case 2:		/* 0xa8-0xaa : 3CH FNUM1 */
			if(r < 0x100)
			{
				UINT32 fn = (((UINT32)(ym2612.OPN.SL3.fn_h&7))<<8) + v;
				UINT8 blk = ym2612.OPN.SL3.fn_h>>3;
				/* keyscale code */
				ym2612.OPN.SL3.kcode[c]= (blk<<2) | opn_fktable[fn >> 7];
				/* phase increment counter */
				ym2612.OPN.SL3.fc[c] = fn_table[fn*2]>>(7-blk);
				ym2612.OPN.SL3.block_fnum[c] = (blk<<11) | fn;
				ym2612.CH[2].SLOT[SLOT1].Incr=-1;
			}
			break;
		case 3:		/* 0xac-0xae : 3CH FNUM2,BLK */
			if(r < 0x100)
				ym2612.OPN.SL3.fn_h = v&0x3f;
			ret = 0;
			break;
		default:
			ret = 0;
			break;
		}
		break;

	case 0xb0:
		switch( OPN_SLOT(r) ){
		case 0:		/* 0xb0-0xb2 : FB,ALGO */
			{
				int feedback = (v>>3)&7;
				CH->ALGO = v&7;
				CH->FB   = feedback ? feedback+6 : 0;
			}
			break;
		case 1:		/* 0xb4-0xb6 : L , R , AMS , PMS (YM2612/YM2610B/YM2610/YM2608) */
			{
				int panshift = c<<1;

				/* b0-2 PMS */
				CH->pms = (v & 7) * 32; /* CH->pms = PM depth * 32 (index in lfo_pm_table) */

				/* b4-5 AMS */
				CH->ams = lfo_ams_depth_shift[(v>>4) & 3];

				/* PAN :  b7 = L, b6 = R */
				ym2612.OPN.pan &= ~(3<<panshift);
				ym2612.OPN.pan |= ((v & 0xc0) >> 6) << panshift; // ..LRLR
			}
			break;
		default:
			ret = 0;
			break;
		}
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}


/*******************************************************************************/
/*      YM2612 local section                                                   */
/*******************************************************************************/

/* User output mask is not emulated chip/checkpoint state. Clearing pan bits
 * here still computes every operator and all feedback/phase/envelope state,
 * including the ARM7 worker. Never use the hardware-disabled flag for mute. */
static unsigned output_pan_mask=4095;
void YM2612SetChannelMask_(unsigned mask){
    unsigned pan=0;
    for(unsigned c=0;c<6;c++)if(mask&(1u<<c))pan|=3u<<(2*c);
    output_pan_mask=pan;
}
/* Generate samples for YM2612 */
ITCM_CODE int YM2612UpdateOne_(s32 *buffer, int length, int stereo, int is_buf_empty)
{
	int pan;
	int active_chs = 0;
	int flags = stereo ? 1:0;
    assert(stereo);fm_last_channel=-1;fm_jobs[0].active=fm_jobs[1].active=0;
    /* Short spans and sparse arrangements run locally: the shared packet
     * and ARM7 completion cost more than the parallel work they expose. */
    unsigned live=0;
    for(unsigned c=0;c<6;c++)live+=!!(ym2612.slot_mask&(15u<<(4*c)));
    fm_worker_channel=live>=5?((ym2612.slot_mask&0x00f000)?3:2):6;


	// if !is_buf_empty, it means it has valid samples to mix with, else it may contain trash
	if (is_buf_empty) memset32(buffer, 0, length<<stereo);

/*
	{
		int c, s;
		ppp();
		for (c = 0; c < 6; c++) {
			int slr = 0, slm;
			printf("%i: ", c);
			for (s = 0; s < 4; s++) {
				if (ym2612.CH[c].SLOT[s].state != EG_OFF) slr = 1;
				printf(" %i", ym2612.CH[c].SLOT[s].state != EG_OFF);
			}
			slm = (ym2612.slot_mask&(0xf<<(c*4))) ? 1 : 0;
			printf(" | %i", slm);
			printf(" | %i\n", ym2612.CH[c].SLOT[SLOT1].Incr==-1);
			if (slr != slm) exit(1);
		}
	}
*/
	/* refresh PG and EG */
	refresh_fc_eg_chan( &ym2612.CH[0] );
	refresh_fc_eg_chan( &ym2612.CH[1] );
	if( (ym2612.OPN.ST.mode & 0xc0) )
		/* 3SLOT MODE */
		refresh_fc_eg_chan_sl3();
	else
		refresh_fc_eg_chan( &ym2612.CH[2] );
	refresh_fc_eg_chan( &ym2612.CH[3] );
	refresh_fc_eg_chan( &ym2612.CH[4] );
	refresh_fc_eg_chan( &ym2612.CH[5] );

	pan = ym2612.OPN.pan & output_pan_mask;

	/* mix to 32bit dest */
	// flags: stereo, ssg_enabled, disabled, _, pan_r, pan_l
	chan_render_prep();
#define	BIT_IF(v,b,c)	{ v &= ~(1<<(b)); if (c) v |= 1<<(b); }
    if(length>32 && FM_WORKER_CHANNEL<6){
        unsigned c=FM_WORKER_CHANNEL,mask=15u<<(4*c);
        BIT_IF(flags,1,(ym2612.ssg_mask&mask)&&(ym2612.OPN.ST.flags&1));
        if(ym2612.slot_mask&mask)active_chs|=chan_render(buffer,length,c,flags|(((pan>>(2*c))&3)<<4))<<c;
    }
    for(unsigned c=0;c<5;c++){
        if(c==FM_WORKER_CHANNEL && length>32)continue;
        unsigned mask=15u<<(4*c);
        BIT_IF(flags,1,(ym2612.ssg_mask&mask)&&(ym2612.OPN.ST.flags&1));
        if(ym2612.slot_mask&mask)active_chs|=chan_render(buffer,length,c,flags|(((pan>>(2*c))&3)<<4))<<c;
    }
    if(fm_last_channel==FM_WORKER_CHANNEL)active_chs|=channel_collect(buffer,length);
	ym2612.OPN.lfo_ampm = crct.pack >> 16; // need_save; now because ch5 might skip updating it
	BIT_IF(flags, 1, (ym2612.ssg_mask & 0xf00000) && (ym2612.OPN.ST.flags & 1));
	if (ym2612.slot_mask & 0xf00000) active_chs |= chan_render(buffer, length, 5, flags|((pan&0xc00)>>6)|(!!ym2612.dacen<<2)) << 5;
#undef	BIT_IF

    active_chs|=channel_collect(buffer,length);
	chan_render_finish(buffer, length, active_chs);

	return active_chs; // 1 if buffer updated
}


/* initialize YM2612 emulator */
void YM2612Init_(int clock, int rate, int flags)
{
	memset(&ym2612, 0, sizeof(ym2612));
	/* A new device lifetime must match cold-process initialization. The
	 * renderer publishes this context even during silent channel blocks;
	 * leaving it from a previous session leaks its envelope/LFO phase. */
	memset(&crct, 0, sizeof(crct));fm_output_demand=0;output_pan_mask=4095;
	init_tables();

	ym2612.OPN.ST.clock = clock;
	ym2612.OPN.ST.rate = rate;
	ym2612.OPN.ST.flags = flags;

	OPNSetPres( 6*24 );

	/* Extend handler */
	YM2612ResetChip_();
}


/* reset */
void YM2612ResetChip_(void)
{
	int i;

	memset(ym2612.REGS, 0, sizeof(ym2612.REGS));

	set_timers( 0x30 ); /* mode 0 , timer reset */
	ym2612.REGS[0x27] = 0x30;

	ym2612.OPN.eg_timer = 0;
	ym2612.OPN.eg_cnt   = 0;
	ym2612.OPN.lfo_inc = 0;
	ym2612.OPN.lfo_cnt = 0;
	ym2612.OPN.lfo_ampm = 126 << 8;
	ym2612.OPN.ST.status = 0;

	reset_channels( &ym2612.CH[0] );
	for(i = 0xb6 ; i >= 0xb4 ; i-- )
	{
		OPNWriteReg(i      ,0xc0);
		OPNWriteReg(i|0x100,0xc0);
		ym2612.REGS[i      ] = 0xc0;
		ym2612.REGS[i|0x100] = 0xc0;
	}
	for(i = 0xb2 ; i >= 0x30 ; i-- )
	{
		OPNWriteReg(i      ,0);
		OPNWriteReg(i|0x100,0);
	}
	for(i = 0x26 ; i >= 0x20 ; i-- ) OPNWriteReg(i,0);
	/* DAC mode clear */
	ym2612.dacen = 0;
	ym2612.dacout = 0;
	ym2612.addr_A1 = 0;
}


/* YM2612 write */
/* a = address */
/* v = value   */
/* returns 1 if sample affecting state changed */
int YM2612Write_(unsigned int a, unsigned int v)
{
	int addr, ret=1;

	v &= 0xff;	/* adjust to 8 bit bus */

	switch( a & 3 ){
	case 0:	/* address port 0 */
	case 2:	/* address port 1 */
		/* reminder: this is not used, see ym2612_write_local() */
		ym2612.OPN.ST.address = v;
		ym2612.addr_A1 = (a & 2) >> 1;
		ret = 0;
		break;

	case 1:
	case 3:	/* data port */
		addr = ym2612.OPN.ST.address | ((int)ym2612.addr_A1 << 8);

		switch( addr & 0x1f0 )
		{
		case 0x20:	/* 0x20-0x2f Mode */
			switch( addr )
			{
			case 0x22:	/* LFO FREQ (YM2608/YM2610/YM2610B/YM2612) */
				if (v&0x08) /* LFO enabled ? */
				{
					ym2612.OPN.lfo_inc = ym2612.OPN.lfo_freq[v&7];
				}
				else
				{
					ym2612.OPN.lfo_inc = 0;
					ym2612.OPN.lfo_cnt = 0;
					ym2612.OPN.lfo_ampm = 126 << 8;
				}
				break;
#if 0 // handled elsewhere
			case 0x24: { // timer A High 8
					int TAnew = (ym2612.OPN.ST.TA & 0x03)|(((int)v)<<2);
					if(ym2612.OPN.ST.TA != TAnew) {
						// we should reset ticker only if new value is written. Outrun requires this.
						ym2612.OPN.ST.TA = TAnew;
						ym2612.OPN.ST.TAC = (1024-TAnew)*18;
						ym2612.OPN.ST.TAT = 0;
					}
				}
				ret=0;
				break;
			case 0x25: { // timer A Low 2
					int TAnew = (ym2612.OPN.ST.TA & 0x3fc)|(v&3);
					if(ym2612.OPN.ST.TA != TAnew) {
						ym2612.OPN.ST.TA = TAnew;
						ym2612.OPN.ST.TAC = (1024-TAnew)*18;
						ym2612.OPN.ST.TAT = 0;
					}
				}
				ret=0;
				break;
			case 0x26: // timer B
				if(ym2612.OPN.ST.TB != v) {
					ym2612.OPN.ST.TB = v;
					ym2612.OPN.ST.TBC  = (256-v)<<4;
					ym2612.OPN.ST.TBC *= 18;
					ym2612.OPN.ST.TBT  = 0;
				}
				ret=0;
				break;
#endif
			case 0x27:	/* mode, timer control */
				set_timers( v );
				ret=0;
				break;
			case 0x28:	/* key on / off */
				{
					UINT8 c;

					c = v & 0x03;
					if( c == 3 ) { ret=0; break; }
					if( v&0x04 ) c+=3;
					if(v&0x10) FM_KEYON(c,SLOT1); else FM_KEYOFF(c,SLOT1);
					if(v&0x20) FM_KEYON(c,SLOT2); else FM_KEYOFF(c,SLOT2);
					if(v&0x40) FM_KEYON(c,SLOT3); else FM_KEYOFF(c,SLOT3);
					if(v&0x80) FM_KEYON(c,SLOT4); else FM_KEYOFF(c,SLOT4);
					break;
				}
			case 0x2a:	/* DAC data (YM2612) */
				ym2612.dacout = ((int)v - 0x80) << DAC_SHIFT;
				ret=0;
				break;
			case 0x2b:	/* DAC Sel  (YM2612) */
				/* b7 = dac enable */
				ym2612.dacen = v & 0x80;
				ret=0;
				break;
			default:
				break;
			}
			break;
		default:	/* 0x30-0xff OPN section */
			/* write register */
			ret = OPNWriteReg(addr,v);
		}
		break;
	}

	return ret;
}

#if 0
UINT8 YM2612Read_(void)
{
	return ym2612.OPN.ST.status;
}

int YM2612PicoTick_(int n)
{
	int ret = 0;

	// timer A
	if(ym2612.OPN.ST.mode & 0x01 && (ym2612.OPN.ST.TAT+=64*n) >= ym2612.OPN.ST.TAC) {
		ym2612.OPN.ST.TAT -= ym2612.OPN.ST.TAC;
		if(ym2612.OPN.ST.mode & 0x04) ym2612.OPN.ST.status |= 1;
		// CSM mode total level latch and auto key on
		if(ym2612.OPN.ST.mode & 0x80) {
			CSMKeyControll( &(ym2612.CH[2]) ); // Vectorman2, etc.
			ret = 1;
		}
	}

	// timer B
	if(ym2612.OPN.ST.mode & 0x02 && (ym2612.OPN.ST.TBT+=64*n) >= ym2612.OPN.ST.TBC) {
		ym2612.OPN.ST.TBT -= ym2612.OPN.ST.TBC;
		if(ym2612.OPN.ST.mode & 0x08) ym2612.OPN.ST.status |= 2;
	}

	return ret;
}
#endif


#include "fm_checkpoint_r22.inc"

bool fm_tables_begin(void){
 if(!ym_tl_tab)ym_tl_tab=malloc(TL_TAB_LEN*sizeof(*ym_tl_tab));
 if(!lfo_pm_table)lfo_pm_table=malloc(128*8*8*sizeof(*lfo_pm_table));
 return ym_tl_tab && lfo_pm_table;
}
void fm_tables_end(void){
 free(ym_tl_tab);free(lfo_pm_table);ym_tl_tab=NULL;lfo_pm_table=NULL;ym_init_tab=0;
}
