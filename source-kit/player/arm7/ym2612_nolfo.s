/*
 * PicoDrive
 * (C) notaz, 2006
 * (C) irixxxx, 2020-2023	added SSG-EG and simple output rate interpolation
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */

@ this is a rewrite of MAME's ym2612 code, in particular this is only the main sample-generatin loop.
@ it does not seem to give much performance increase (if any at all), so don't use it if it causes trouble.
@ - notaz, 2006

@ vim:filetype=armasm

#include <pico/arm_features.h>

@ very simple YM2612 output rate to sample rate adaption (~500k cycles @44100)
#define SSG_EG

.equiv SLOT1, 0
.equiv SLOT2, 2
.equiv SLOT3, 1
.equiv SLOT4, 3
.equiv SLOT_STRUCT_SIZE, 0x38

.equiv TL_TAB_LEN, 0x1A00

.equiv EG_ATT, 4
.equiv EG_DEC, 3
.equiv EG_SUS, 2
.equiv EG_REL, 1
.equiv EG_OFF, 0

.equiv EG_SH,		  16             @ 16.16 fixed point (envelope generator timing)
.equiv LFO_SH,            24  /*  8.24 fixed point (LFO calculations)       */

.equiv ENV_QUIET,	  (2*13*256/8)

.text
.align 2
    PIC_LDR_INIT()


.macro compact_volume_base reg
    cmp     \reg, #ENV_QUIET
    movhs   \reg, #0
    biclo   \reg, \reg, #1
    addlo   \reg, r12, \reg, lsl #3
    addlo   \reg, \reg, #512
.endm

@ r5=slot, r1=eg_cnt, trashes: r0,r2,r3
@ writes output to routp, but only if vol_out changes
.macro update_eg_phase_slot
    ldrb    r2, [r5,#0x17]       @ state
    add     r3, r5, #0x1c
    tst     r2, r2
    beq     0f                   @ EG_OFF

    ldr     r2, [r3, r2, lsl #2] @ pack
    mov     r3, #1
    mov     r0, r2, lsr #24      @ shift
    mov     r3, r3, lsl r0
    sub     r3, r3, #1

    tst     r1, r3
    bne     0f                   @ no volume change

    mov     r3, r1, lsr r0
    and     r3, r3, #7
    add     r3, r3, r3, lsl #1
    mov     r3, r2, lsr r3
    and     r3, r3, #7           @ eg_inc_val shift, may be 0
    ldrb    r2, [r5,#0x17]       @ state

    @ non-SSG-EG mode
    cmp     r2, #4               @ EG_ATT
    ldrh    r0, [r5,#0x1a]       @ volume, unsigned (0-1023)
    beq     4f

    cmp     r2, #2
    mov     r2, #1
    mov     r2, r2, lsl r3
    add     r0, r0, r2, lsr #1   @ volume += eg_inc_val
    blt     1f                   @ EG_REL
    beq     2f                   @ EG_SUS

3:  @ EG_DEC
    ldr     r2, [r5,#0x1c]       @ sl (can be 16bit?)
    mov     r3, #EG_SUS
    cmp     r0, r2               @ if ( volume >= (INT32) SLOT->sl )
    strgeb  r3, [r5,#0x17]       @ state
    b       10f

4:  @ EG_ATT
    subs    r3, r3, #1           @ eg_inc_val_shift - 1
    mvnpl   r2, r0
    movpl   r2, r2, lsl r3
    addpl   r0, r0, r2, asr #4
    cmp     r0, #0               @ if (volume <= MIN_ATT_INDEX)
    bgt     10f
    ldr     r2, [r5,#0x1c]
    mov     r0, #0
    cmp     r2, #0
    movne   r3, #EG_DEC
    moveq   r3, #EG_SUS
    strb    r3, [r5,#0x17]       @ state
    b       10f

2:  @ EG_SUS
    mov     r2, #1024
    sub     r2, r2, #1           @ r2 = MAX_ATT_INDEX
    cmp     r0, r2               @ if ( volume >= MAX_ATT_INDEX )
    movge   r0, r2
    b       10f

1:  @ EG_REL
    mov     r2, #1024
    sub     r2, r2, #1           @ r2 = MAX_ATT_INDEX
    cmp     r0, r2               @ if ( volume >= MAX_ATT_INDEX )
    movge   r0, r2
    movge   r3, #EG_OFF
    strgeb  r3, [r5,#0x17]       @ state

10: @ finish
    ldrh    r3, [r5,#0x18]       @ tl
    strh    r0, [r5,#0x1a]       @ volume
    add     r0, r0, r3
    strh    r0, [r5,#0x34]       @ vol_out
    compact_volume_base r0
    str     r0, [r7]            @ same context volume cache
0:
.endm

#if defined(SSG_EG)
@ r5=slot, trashes: r0,r2,r3
.macro update_ssg_eg
    ldrh    r0, [r5,#0x30]                @ ssg+ssgn
    ldrb    r2, [r5,#0x17]                @ state
    ldrh    r3, [r5,#0x1a]                @ volume
    tst     r0, #0x08                     @ ssg enabled &&
    beq     10f
    cmp     r2, #EG_REL+1                 @   state > EG_REL &&
    cmpge   r3, #0x200                    @   volume >= 0x200?
    blt     10f

    tst     r0, #0x01
    beq     1f

    tst     r0, #0x02
    eorne   r0, r0, r0, lsr #8            @ ssg ^= ssgn ^ 4
    eorne   r0, r0, #0x4
    orrne   r0, r0, #0x400                @ ssgn = 4
    strneh  r0, [r5,#0x30]

    tst     r0, #0x4                      @ if ( !(ssg&0x04) )
    bne     9f
    cmp     r2, #EG_ATT                   @ if ( state != EG_ATT )
    movne   r3, #0x400
    subne   r3, r3, #1
    strneh  r3, [r5,#0x1a]                @ volume = MAX_ATT
    b       9f

1:  tst     r0, #0x02
    eorne   r0, r0, #0x4                  @ ssg ^= 4
    eorne   r0, r0, #0x400                @ ssgn ^= 4
    strneh  r0, [r5,#0x30]
    moveq   r0, #0
    streq   r0, [lr,#0x10]                @ phase = 0

    cmp     r2, #EG_ATT                   @ if ( state != EG_ATT )
    beq     9f

    ldr     r0, [r5,#0x1c]                @ sl
    mov     r2, #EG_SUS                   @ state = sl==MIN_ATT ? EG_SUS:EG_DEC
    cmp     r0, #0

    ldrh    r0, [r5,#0x32]                @ ar+ksr
    movne   r2, #EG_DEC
    cmp     r0, #32+62                    @ if ( ar+ksr >= 32+62 )
    movge   r3, #0
    strgeh  r3, [r5,#0x1a]                @ volume = MIN_ATT
    bge     8f

    cmp     r3, #0
    movgt   r2, #EG_ATT
8:  strb    r2, [r5,#0x17]                @ state
9:
    ldrb    r0, [r5,#0x30]                @ ssg
    ldrh    r2, [r5,#0x18]                @ tl
    recalc_volume_ssg r3 r0
    add     r3, r3, r2                    @ volume += tl
    strh    r3, [r5,#0x34]                @ vol_out
    str     r3, [r7]       @ exact SSG volume
10:
.endm

@ r5=slot
.macro recalc_volume_ssg vol ssg
@    and     \ssg, \ssg, #0x0c
    cmp     \ssg, #0x0c                   @ if (~ssg&0x0c)
    rsbge   \vol, \vol, #0x200            @ volume = (0x200-volume) & MAX_ATT
    movge   \vol, \vol, lsl #22
    movge   \vol, \vol, lsr #22
.endm
#endif

@ r12=lfo_ampm[31:16], r1=lfo_cnt_old, r2=lfo_cnt, r3=scratch
.macro advance_lfo_m
    mov     r2, r2, lsr #LFO_SH
    cmp     r2, r1, lsr #LFO_SH
    beq     0f
    and     r3, r2, #0x3f
    tst     r2, #0x40
    eoreq   r3, r3, #0x3f
    bic     r12,r12, #0xff000000          @ lfo_ampm &= 0xff
    orr     r12,r12, r3, lsl #1+24

    mov     r2, r2, lsr #2
    cmp     r2, r1, lsr #LFO_SH+2
    andne   r3, r2, #0x1f
    bicne   r12,r12, #0xff0000
    orrne   r12,r12, r3, lsl #16

0:
.endm


@ result goes to r1, trashes r2
.macro make_eg_out slot
.if     \slot == SLOT1
    mov     r1, r6, lsl #16
    mov     r1, r1, lsr #16
.elseif \slot == SLOT2
    mov     r1, r6, lsr #16
.elseif \slot == SLOT3
    mov     r1, r7, lsl #16
    mov     r1, r1, lsr #16
.elseif \slot == SLOT4
    mov     r1, r7, lsr #16
.endif
.endm


@ \r=sin/result, r1=env, r3=ym_tl_tab
.macro lookup_tl r row
.ifc \row,r6
    ldr     r1, [lr, #0x60]
.endif
.ifc \row,r7
    ldr     r1, [lr, #0x64]
.endif
.ifc \row,r8
    ldr     r1, [lr, #0x68]
.endif
.ifc \row,r9
    ldr     r1, [lr, #0x6c]
.endif
    cmp     r1, #0
    moveq   \r, #0
    beq     99f
    @ r12 retains the compact table base.
    tst     \r, #0x100
    eorne   \r, \r, #0xff
    tst     \r, #0x200
    and     \r, \r, #0xff

    add     \r, r12, \r, lsl #1
    ldrh    \r, [\r]
    add     \r, r1, \r, lsl #1
    ldrh    \r, [\r]
    rsbne   \r, \r, #0
99:
.endm


@ lr=context, r12=pack (stereo, ssg_enabled, disabled, lfo_enabled | pan_r, pan_l, ams[2] | AMmasks[4] | FB[4] | lfo_ampm[16])
@ r0-r2=scratch, r3=sin_tab, r5=scratch, r6-r7=vol_out[4], r10=op1_out
.macro upd_algo0_m

    @ SLOT3
    ldr     r0, [lr, #0x38] @ mem (signed)
    mov     r0, r0, asr #1
    add     r0, r0, r8, lsr #16
    lookup_tl r0 r8                  @ r0=c2

0:
    @ SLOT4
    mov     r0, r0, asr #1
    add     r0, r0, r9, lsr #16
    lookup_tl r0 r9                  @ r0=output smp

1:
    @ SLOT2
    mov     r5, r10, asr #17
    add     r2, r5, r7, lsr #16
    lookup_tl r2 r7                  @ r2=mem

2:
    str     r2, [lr, #0x38] @ mem
.endm


.macro upd_algo1_m

    @ SLOT3
    ldr     r0, [lr, #0x38] @ mem (signed)
    mov     r0, r0, asr #1
    add     r0, r0, r8, lsr #16
    lookup_tl r0 r8                 @ r0=c2

0:
    @ SLOT4
    mov     r0, r0, asr #1
    add     r0, r0, r9, lsr #16
    lookup_tl r0 r9                 @ r0=output smp

1:
    @ SLOT2
    mov     r2, r7, lsr #16
    lookup_tl r2 r7                 @ r2=mem

2:
    add     r2, r2, r10, asr #16
    str     r2, [lr, #0x38]
.endm


.macro upd_algo2_m

    @ SLOT3
    ldr     r0, [lr, #0x38] @ mem (signed)
    mov     r0, r0, asr #1
    add     r0, r0, r8, lsr #16
    lookup_tl r0 r8                 @ r0=c2

0:
    add     r0, r0, r10, asr #16

    @ SLOT4
    mov     r0, r0, asr #1
    add     r0, r0, r9, lsr #16
    lookup_tl r0 r9                 @ r0=output smp

1:
    @ SLOT2
    mov     r2, r7, lsr #16
    lookup_tl r2 r7                 @ r2=mem

2:
    str     r2, [lr, #0x38] @ mem
.endm


.macro upd_algo3_m

    @ SLOT3
    ldr     r2, [lr, #0x38] @ mem (for future)
    mov     r0, r8, lsr #16
    lookup_tl r0 r8                 @ r0=c2

0:
    add     r0, r0, r2

    @ SLOT4
    mov     r0, r0, asr #1
    add     r0, r0, r9, lsr #16
    lookup_tl r0 r9                 @ r0=output smp

1:
    @ SLOT2
    mov     r5, r10, asr #17
    add     r2, r5, r7, lsr #16
    lookup_tl r2 r7                 @ r2=mem

2:
    str     r2, [lr, #0x38]      @ mem
.endm


.macro upd_algo4_m

    @ SLOT3
    mov     r0, r8, lsr #16
    lookup_tl r0 r8                 @ r0=c2

0:
    @ SLOT4
    mov     r0, r0, asr #1
    add     r0, r0, r9, lsr #16
    lookup_tl r0 r9                 @ r0=output smp

1:
    @ SLOT2
    mov     r5, r10, asr #17
    add     r2, r5, r7, lsr #16
    lookup_tl r2 r7
    add     r0, r0, r2            @ add to smp

2:
.endm


.macro upd_algo5_m

    @ SLOT3
    ldr     r0, [lr, #0x38] @ mem (signed)
    mov     r0, r0, asr #1
    add     r0, r0, r8, lsr #16
    lookup_tl r0 r8                 @ r0=output smp

0:
    @ SLOT4
    mov     r5, r10, asr #17
    add     r2, r5, r9, lsr #16
    lookup_tl r2 r9
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    mov     r5, r10, asr #17
    add     r2, r5, r7, lsr #16
    lookup_tl r2 r7
    add     r0, r0, r2           @ add to smp

2:
    mov     r1, r10, asr #16
    str     r1, [lr, #0x38] @ mem
.endm


.macro upd_algo6_m

    @ SLOT3
    mov     r0, r8, lsr #16
    lookup_tl r0 r8                 @ r0=output smp

0:
    @ SLOT4
    mov     r2, r9, lsr #16
    lookup_tl r2 r9
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    mov     r5, r10, asr #17
    add     r2, r5, r7, lsr #16
    lookup_tl r2 r7
    add     r0, r0, r2           @ add to smp

2:
.endm


.macro upd_algo7_m

    @ SLOT3
    mov     r0, r8, lsr #16
    lookup_tl r0 r8                 @ r0=output smp

0:
    add     r0, r0, r10, asr #16

    @ SLOT4
    mov     r2, r9, lsr #16
    lookup_tl r2 r9
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    mov     r2, r7, lsr #16
    lookup_tl r2 r7
    add     r0, r0, r2           @ add to smp

2:
.endm


.macro upd_slot1_m

    ands    r2, r4, #0xf0000
    moveq   r0, #0
    movne   r2, r2, lsr #16
    addne   r0, r10, r10, lsl #16
    movne   r0, r0, asr #16
    movne   r0, r0, lsl r2

    add     r0, r0, r6
    mov     r0, r0, lsr #16
    lookup_tl r0 r6
    mov     r10,r10,lsl #16     @ ct->op1_out <<= 16;
    mov     r0, r0, lsl #16
    orr     r10,r10, r0, lsr #16

0:
.endm


@ lr=context, r12=pack (stereo, ssg_enabled, disabled, lfo_enabled | pan_r, pan_l, ams[2] | AMmasks[4] | FB[4] | lfo_ampm[16])
@ r0-r2=scratch, r3=sin_tab/scratch, r4=(length<<8)|dac,upd_cnt[3],was_update,algo[3], r5=tl_tab/slot,
@ r6-r7=vol_out[4], r8=eg_timer, r9=eg_timer_add[31:16], r10=op1_out, r11=buffer
.global chan_render_loop_nolfo
chan_render_loop_nolfo:
    cmp     r2, #128
    bhi     chan_render_loop_nolfo_fallback
    ldr     r3, [r0, #0x4c]
    tst     r3, #14
    bne     chan_render_loop_nolfo_fallback
    ldr r3,[r0,#0x50]
    tst r3,#0x40
    bne chan_render_loop_nolfo_fallback
    stmfd   sp!, {r4-r11,lr}
    mov     lr,  r0
    mov     r4,  r2, lsl #8      @ no more 24 bits here
    ldr     r12, [lr, #0x4c]
    ldr     r0,  [lr, #0x50]
    mov     r11, r1
    and     r0,  r0, #0xf7
    orr     r4,  r4, r0          @ (length<<8)|dac,upd_cnt[2],unused,algo[3]
    and     r3, r12, #0xf000
    orr     r4, r4, r3, lsl #4
    and     r3, r12, #0x31
    orr     r4, r4, r3, lsl #20
    PIC_LDR(r12, r3, ym_lookup)
    ldr     r8, [lr, #0x44]      @ eg_timer
    ldr     r9, [lr, #0x48]      @ eg_timer_add
    ldr     r10, [lr, #0x54]     @ op1_out
    ldr     r5, [lr, #0x40]
    ldrh    r6, [r5, #0x34+SLOT_STRUCT_SIZE*0]
    ldrh    r7, [r5, #0x34+SLOT_STRUCT_SIZE*2]
    ldrh    r8, [r5, #0x34+SLOT_STRUCT_SIZE*1]
    ldrh    r9, [r5, #0x34+SLOT_STRUCT_SIZE*3]
    compact_volume_base r6
    compact_volume_base r7
    compact_volume_base r8
    compact_volume_base r9
    add     r0, lr, #0x60
    stmia   r0, {r6-r9}

    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
    and     r0, r4, #7
    PIC_XB(,r0, lsl #2)
    nop
    PIC_BT(fast_loop_0)
    PIC_BT(fast_loop_1)
    PIC_BT(fast_loop_2)
    PIC_BT(fast_loop_3)
    PIC_BT(fast_loop_4)
    PIC_BT(fast_loop_5)
    PIC_BT(fast_loop_6)
    PIC_BT(fast_loop_7)
    .pool
fast_loop_0:
    sub r4,r4,#0x100
    tst r4,#0x8000
    bne crl_loop_end
    @ -- EG --
    and r0,r4,#0x30
    bic r4,r4,#0x30
    cmp r0,#0x20
    orrlo r4,r4,#0x70           @ two EG ticks, then countdown3
    subhs r0,r0,#0x10
    orrhs r4,r4,r0              @ one EG tick, countdown1 or2
repeat_eg_0:

    add     r0, lr, #0x10
    stmia   r0, {r6-r9}
    ldr     r1, [lr, #0x3c]      @ eg_cnt
    add     r1, r1, #1
    cmp     r1, #4096
    movge   r1, #1
    str     r1, [lr, #0x3c]
    ldr     r0, [lr, #0x58]
    cmp     r0, #0
    bne     eg_done_0

    ldr     r5, [lr, #0x40]
    add     r7, lr, #0x60
    mov     r6, #4
eg_upd_loop_0:
    update_eg_phase_slot
    add     r7, r7, #4
#if 0
    subs    r6, r6, #1
    addne   r5, r5, #SLOT_STRUCT_SIZE
#else
    add     r5, r5, #SLOT_STRUCT_SIZE*2
    update_eg_phase_slot
    add     r7, r7, #4
    subs    r6, r6, #2
    subne   r5, r5, #SLOT_STRUCT_SIZE
#endif
    bne     eg_upd_loop_0
    sub     r5, r5, #SLOT_STRUCT_SIZE*3

    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
eg_done_0:
    tst r4,#0x40
    bicne r4,r4,#0x40
    bne repeat_eg_0
    upd_slot1_m
    upd_algo0_m
crl_algo_done_0:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3,r5}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    add     r9, r9, r5

crl_smp_loop_end_0:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip_0
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    tst     r4, #0x100000
    beq     ctl_sample_mono_0

    tst     r4, #0x2000000              @ L
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    tst     r4, #0x1000000              @ R
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    b       fast_loop_0

ctl_sample_mono_0:
    str     r0, [r11], #4
    b       fast_loop_0

ctl_sample_skip_0:
    mov     r1, r4, lsr #20
    and     r1, r1, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_0

    .pool
fast_loop_1:
    sub r4,r4,#0x100
    tst r4,#0x8000
    bne crl_loop_end
    @ -- EG --
    and r0,r4,#0x30
    bic r4,r4,#0x30
    cmp r0,#0x20
    orrlo r4,r4,#0x70           @ two EG ticks, then countdown3
    subhs r0,r0,#0x10
    orrhs r4,r4,r0              @ one EG tick, countdown1 or2
repeat_eg_1:

    add     r0, lr, #0x10
    stmia   r0, {r6-r9}
    ldr     r1, [lr, #0x3c]      @ eg_cnt
    add     r1, r1, #1
    cmp     r1, #4096
    movge   r1, #1
    str     r1, [lr, #0x3c]
    ldr     r0, [lr, #0x58]
    cmp     r0, #0
    bne     eg_done_1

    ldr     r5, [lr, #0x40]
    add     r7, lr, #0x60
    mov     r6, #4
eg_upd_loop_1:
    update_eg_phase_slot
    add     r7, r7, #4
#if 0
    subs    r6, r6, #1
    addne   r5, r5, #SLOT_STRUCT_SIZE
#else
    add     r5, r5, #SLOT_STRUCT_SIZE*2
    update_eg_phase_slot
    add     r7, r7, #4
    subs    r6, r6, #2
    subne   r5, r5, #SLOT_STRUCT_SIZE
#endif
    bne     eg_upd_loop_1
    sub     r5, r5, #SLOT_STRUCT_SIZE*3

    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
eg_done_1:
    tst r4,#0x40
    bicne r4,r4,#0x40
    bne repeat_eg_1
    upd_slot1_m
    upd_algo1_m
crl_algo_done_1:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3,r5}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    add     r9, r9, r5

crl_smp_loop_end_1:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip_1
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    tst     r4, #0x100000
    beq     ctl_sample_mono_1

    tst     r4, #0x2000000              @ L
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    tst     r4, #0x1000000              @ R
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    b       fast_loop_1

ctl_sample_mono_1:
    str     r0, [r11], #4
    b       fast_loop_1

ctl_sample_skip_1:
    mov     r1, r4, lsr #20
    and     r1, r1, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_1

    .pool
fast_loop_2:
    sub r4,r4,#0x100
    tst r4,#0x8000
    bne crl_loop_end
    @ -- EG --
    and r0,r4,#0x30
    bic r4,r4,#0x30
    cmp r0,#0x20
    orrlo r4,r4,#0x70           @ two EG ticks, then countdown3
    subhs r0,r0,#0x10
    orrhs r4,r4,r0              @ one EG tick, countdown1 or2
repeat_eg_2:

    add     r0, lr, #0x10
    stmia   r0, {r6-r9}
    ldr     r1, [lr, #0x3c]      @ eg_cnt
    add     r1, r1, #1
    cmp     r1, #4096
    movge   r1, #1
    str     r1, [lr, #0x3c]
    ldr     r0, [lr, #0x58]
    cmp     r0, #0
    bne     eg_done_2

    ldr     r5, [lr, #0x40]
    add     r7, lr, #0x60
    mov     r6, #4
eg_upd_loop_2:
    update_eg_phase_slot
    add     r7, r7, #4
#if 0
    subs    r6, r6, #1
    addne   r5, r5, #SLOT_STRUCT_SIZE
#else
    add     r5, r5, #SLOT_STRUCT_SIZE*2
    update_eg_phase_slot
    add     r7, r7, #4
    subs    r6, r6, #2
    subne   r5, r5, #SLOT_STRUCT_SIZE
#endif
    bne     eg_upd_loop_2
    sub     r5, r5, #SLOT_STRUCT_SIZE*3

    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
eg_done_2:
    tst r4,#0x40
    bicne r4,r4,#0x40
    bne repeat_eg_2
    upd_slot1_m
    upd_algo2_m
crl_algo_done_2:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3,r5}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    add     r9, r9, r5

crl_smp_loop_end_2:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip_2
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    tst     r4, #0x100000
    beq     ctl_sample_mono_2

    tst     r4, #0x2000000              @ L
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    tst     r4, #0x1000000              @ R
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    b       fast_loop_2

ctl_sample_mono_2:
    str     r0, [r11], #4
    b       fast_loop_2

ctl_sample_skip_2:
    mov     r1, r4, lsr #20
    and     r1, r1, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_2

    .pool
fast_loop_3:
    sub r4,r4,#0x100
    tst r4,#0x8000
    bne crl_loop_end
    @ -- EG --
    and r0,r4,#0x30
    bic r4,r4,#0x30
    cmp r0,#0x20
    orrlo r4,r4,#0x70           @ two EG ticks, then countdown3
    subhs r0,r0,#0x10
    orrhs r4,r4,r0              @ one EG tick, countdown1 or2
repeat_eg_3:

    add     r0, lr, #0x10
    stmia   r0, {r6-r9}
    ldr     r1, [lr, #0x3c]      @ eg_cnt
    add     r1, r1, #1
    cmp     r1, #4096
    movge   r1, #1
    str     r1, [lr, #0x3c]
    ldr     r0, [lr, #0x58]
    cmp     r0, #0
    bne     eg_done_3

    ldr     r5, [lr, #0x40]
    add     r7, lr, #0x60
    mov     r6, #4
eg_upd_loop_3:
    update_eg_phase_slot
    add     r7, r7, #4
#if 0
    subs    r6, r6, #1
    addne   r5, r5, #SLOT_STRUCT_SIZE
#else
    add     r5, r5, #SLOT_STRUCT_SIZE*2
    update_eg_phase_slot
    add     r7, r7, #4
    subs    r6, r6, #2
    subne   r5, r5, #SLOT_STRUCT_SIZE
#endif
    bne     eg_upd_loop_3
    sub     r5, r5, #SLOT_STRUCT_SIZE*3

    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
eg_done_3:
    tst r4,#0x40
    bicne r4,r4,#0x40
    bne repeat_eg_3
    upd_slot1_m
    upd_algo3_m
crl_algo_done_3:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3,r5}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    add     r9, r9, r5

crl_smp_loop_end_3:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip_3
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    tst     r4, #0x100000
    beq     ctl_sample_mono_3

    tst     r4, #0x2000000              @ L
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    tst     r4, #0x1000000              @ R
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    b       fast_loop_3

ctl_sample_mono_3:
    str     r0, [r11], #4
    b       fast_loop_3

ctl_sample_skip_3:
    mov     r1, r4, lsr #20
    and     r1, r1, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_3

    .pool
fast_loop_4:
    sub r4,r4,#0x100
    tst r4,#0x8000
    bne crl_loop_end
    @ -- EG --
    and r0,r4,#0x30
    bic r4,r4,#0x30
    cmp r0,#0x20
    orrlo r4,r4,#0x70           @ two EG ticks, then countdown3
    subhs r0,r0,#0x10
    orrhs r4,r4,r0              @ one EG tick, countdown1 or2
repeat_eg_4:

    add     r0, lr, #0x10
    stmia   r0, {r6-r9}
    ldr     r1, [lr, #0x3c]      @ eg_cnt
    add     r1, r1, #1
    cmp     r1, #4096
    movge   r1, #1
    str     r1, [lr, #0x3c]
    ldr     r0, [lr, #0x58]
    cmp     r0, #0
    bne     eg_done_4

    ldr     r5, [lr, #0x40]
    add     r7, lr, #0x60
    mov     r6, #4
eg_upd_loop_4:
    update_eg_phase_slot
    add     r7, r7, #4
#if 0
    subs    r6, r6, #1
    addne   r5, r5, #SLOT_STRUCT_SIZE
#else
    add     r5, r5, #SLOT_STRUCT_SIZE*2
    update_eg_phase_slot
    add     r7, r7, #4
    subs    r6, r6, #2
    subne   r5, r5, #SLOT_STRUCT_SIZE
#endif
    bne     eg_upd_loop_4
    sub     r5, r5, #SLOT_STRUCT_SIZE*3

    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
eg_done_4:
    tst r4,#0x40
    bicne r4,r4,#0x40
    bne repeat_eg_4
    upd_slot1_m
    upd_algo4_m
crl_algo_done_4:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3,r5}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    add     r9, r9, r5

crl_smp_loop_end_4:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip_4
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    tst     r4, #0x100000
    beq     ctl_sample_mono_4

    tst     r4, #0x2000000              @ L
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    tst     r4, #0x1000000              @ R
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    b       fast_loop_4

ctl_sample_mono_4:
    str     r0, [r11], #4
    b       fast_loop_4

ctl_sample_skip_4:
    mov     r1, r4, lsr #20
    and     r1, r1, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_4

    .pool
fast_loop_5:
    sub r4,r4,#0x100
    tst r4,#0x8000
    bne crl_loop_end
    @ -- EG --
    and r0,r4,#0x30
    bic r4,r4,#0x30
    cmp r0,#0x20
    orrlo r4,r4,#0x70           @ two EG ticks, then countdown3
    subhs r0,r0,#0x10
    orrhs r4,r4,r0              @ one EG tick, countdown1 or2
repeat_eg_5:

    add     r0, lr, #0x10
    stmia   r0, {r6-r9}
    ldr     r1, [lr, #0x3c]      @ eg_cnt
    add     r1, r1, #1
    cmp     r1, #4096
    movge   r1, #1
    str     r1, [lr, #0x3c]
    ldr     r0, [lr, #0x58]
    cmp     r0, #0
    bne     eg_done_5

    ldr     r5, [lr, #0x40]
    add     r7, lr, #0x60
    mov     r6, #4
eg_upd_loop_5:
    update_eg_phase_slot
    add     r7, r7, #4
#if 0
    subs    r6, r6, #1
    addne   r5, r5, #SLOT_STRUCT_SIZE
#else
    add     r5, r5, #SLOT_STRUCT_SIZE*2
    update_eg_phase_slot
    add     r7, r7, #4
    subs    r6, r6, #2
    subne   r5, r5, #SLOT_STRUCT_SIZE
#endif
    bne     eg_upd_loop_5
    sub     r5, r5, #SLOT_STRUCT_SIZE*3

    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
eg_done_5:
    tst r4,#0x40
    bicne r4,r4,#0x40
    bne repeat_eg_5
    upd_slot1_m
    upd_algo5_m
crl_algo_done_5:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3,r5}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    add     r9, r9, r5

crl_smp_loop_end_5:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip_5
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    tst     r4, #0x100000
    beq     ctl_sample_mono_5

    tst     r4, #0x2000000              @ L
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    tst     r4, #0x1000000              @ R
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    b       fast_loop_5

ctl_sample_mono_5:
    str     r0, [r11], #4
    b       fast_loop_5

ctl_sample_skip_5:
    mov     r1, r4, lsr #20
    and     r1, r1, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_5

    .pool
fast_loop_6:
    sub r4,r4,#0x100
    tst r4,#0x8000
    bne crl_loop_end
    @ -- EG --
    and r0,r4,#0x30
    bic r4,r4,#0x30
    cmp r0,#0x20
    orrlo r4,r4,#0x70           @ two EG ticks, then countdown3
    subhs r0,r0,#0x10
    orrhs r4,r4,r0              @ one EG tick, countdown1 or2
repeat_eg_6:

    add     r0, lr, #0x10
    stmia   r0, {r6-r9}
    ldr     r1, [lr, #0x3c]      @ eg_cnt
    add     r1, r1, #1
    cmp     r1, #4096
    movge   r1, #1
    str     r1, [lr, #0x3c]
    ldr     r0, [lr, #0x58]
    cmp     r0, #0
    bne     eg_done_6

    ldr     r5, [lr, #0x40]
    add     r7, lr, #0x60
    mov     r6, #4
eg_upd_loop_6:
    update_eg_phase_slot
    add     r7, r7, #4
#if 0
    subs    r6, r6, #1
    addne   r5, r5, #SLOT_STRUCT_SIZE
#else
    add     r5, r5, #SLOT_STRUCT_SIZE*2
    update_eg_phase_slot
    add     r7, r7, #4
    subs    r6, r6, #2
    subne   r5, r5, #SLOT_STRUCT_SIZE
#endif
    bne     eg_upd_loop_6
    sub     r5, r5, #SLOT_STRUCT_SIZE*3

    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
eg_done_6:
    tst r4,#0x40
    bicne r4,r4,#0x40
    bne repeat_eg_6
    upd_slot1_m
    upd_algo6_m
crl_algo_done_6:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3,r5}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    add     r9, r9, r5

crl_smp_loop_end_6:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip_6
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    tst     r4, #0x100000
    beq     ctl_sample_mono_6

    tst     r4, #0x2000000              @ L
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    tst     r4, #0x1000000              @ R
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    b       fast_loop_6

ctl_sample_mono_6:
    str     r0, [r11], #4
    b       fast_loop_6

ctl_sample_skip_6:
    mov     r1, r4, lsr #20
    and     r1, r1, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_6

    .pool
fast_loop_7:
    sub r4,r4,#0x100
    tst r4,#0x8000
    bne crl_loop_end
    @ -- EG --
    and r0,r4,#0x30
    bic r4,r4,#0x30
    cmp r0,#0x20
    orrlo r4,r4,#0x70           @ two EG ticks, then countdown3
    subhs r0,r0,#0x10
    orrhs r4,r4,r0              @ one EG tick, countdown1 or2
repeat_eg_7:

    add     r0, lr, #0x10
    stmia   r0, {r6-r9}
    ldr     r1, [lr, #0x3c]      @ eg_cnt
    add     r1, r1, #1
    cmp     r1, #4096
    movge   r1, #1
    str     r1, [lr, #0x3c]
    ldr     r0, [lr, #0x58]
    cmp     r0, #0
    bne     eg_done_7

    ldr     r5, [lr, #0x40]
    add     r7, lr, #0x60
    mov     r6, #4
eg_upd_loop_7:
    update_eg_phase_slot
    add     r7, r7, #4
#if 0
    subs    r6, r6, #1
    addne   r5, r5, #SLOT_STRUCT_SIZE
#else
    add     r5, r5, #SLOT_STRUCT_SIZE*2
    update_eg_phase_slot
    add     r7, r7, #4
    subs    r6, r6, #2
    subne   r5, r5, #SLOT_STRUCT_SIZE
#endif
    bne     eg_upd_loop_7
    sub     r5, r5, #SLOT_STRUCT_SIZE*3

    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
eg_done_7:
    tst r4,#0x40
    bicne r4,r4,#0x40
    bne repeat_eg_7
    upd_slot1_m
    upd_algo7_m
crl_algo_done_7:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3,r5}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    add     r9, r9, r5

crl_smp_loop_end_7:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip_7
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    tst     r4, #0x100000
    beq     ctl_sample_mono_7

    tst     r4, #0x2000000              @ L
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    tst     r4, #0x1000000              @ R
    addeq   r11, r11, #4
    strne   r0, [r11], #4
    b       fast_loop_7

ctl_sample_mono_7:
    str     r0, [r11], #4
    b       fast_loop_7

ctl_sample_skip_7:
    mov     r1, r4, lsr #20
    and     r1, r1, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_7

    .pool
crl_loop_end:
    add     r3, lr, #0x10
    stmia   r3, {r6-r9}
    ldr     r5, [lr, #0x40]
    ldrh    r6, [r5, #0x34+SLOT_STRUCT_SIZE*0]
    ldrh    r7, [r5, #0x34+SLOT_STRUCT_SIZE*2]
    ldrh    r8, [r5, #0x34+SLOT_STRUCT_SIZE*1]
    ldrh    r9, [r5, #0x34+SLOT_STRUCT_SIZE*3]
    add     r0, lr, #0x60
    stmia   r0, {r6-r9}
    @ Native EG timer is unchanged; r8 now holds an operator row.
    @ Original pack is unchanged; remove packed constants from final algo.
    mvn     r1, #0xff
    orr     r4, r4, r1
    str     r4,  [lr, #0x50]     @ was_update
    str     r10, [lr, #0x54]     @ op1_out
    ldmfd   sp!, {r4-r11,pc}

.pool

@ vim:filetype=armasm
