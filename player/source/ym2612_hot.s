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

.section .itcm,"ax",%progbits
.align 2
    PIC_LDR_INIT()

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
    ldrb    r0, [r5,#0x30]       @ ssg
    and     r3, r3, #7
    add     r3, r3, r3, lsl #1
    mov     r3, r2, lsr r3
    and     r3, r3, #7           @ eg_inc_val shift, may be 0
    ldrb    r2, [r5,#0x17]       @ state

#if defined(SSG_EG)
    tst     r0, #0x08            @ ssg enabled?
    tstne   r12, #0x02
    bne     9f
#endif

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
#if defined(SSG_EG)
    b       11f

9:  @ SSG-EG mode
    ldrh    r0, [r5,#0x1a]       @ volume, unsigned (0-1023)
    cmp     r2, #4               @ EG_ATT
    beq     4f

    cmp     r0, #0x200           @ if ( volume < 0x200 )
    movlt   r0, #1
    movlt   r3, r0, lsl r3
    ldrlth  r0, [r5,#0x1a]       @ volume, unsigned (0-1023)
    movlt   r3, r3, lsr #1       @ eg_inc_val
    addlt   r0, r0, r3, lsl #2   @ ...*4

    cmp     r2, #2
    blt     1f                   @ EG_REL
    beq     10f                  @ EG_SUS - nothing more to do

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
    ldr     r2, [r5,#0x1c]       @ sl
    mov     r0, #0
    cmp     r2, #0
    movne   r3, #EG_DEC
    moveq   r3, #EG_SUS
    strb    r3, [r5,#0x17]       @ state
    b       10f

1:  @ EG_REL
    cmp     r0, #0x200           @ if ( volume >= 0x200 )
    movge   r0, #1024
    subge   r0, r0, #1
    movge   r3, #EG_OFF
    strgeb  r3, [r5,#0x17]       @ state
    strh    r0, [r5,#0x1a]       @ preserve updated SSG release volume
    ldrh    r3, [r5,#0x18]       @ tl
    b       11f

10: @ finish
    ldrb    r2, [r5,#0x30]       @ ssg
    ldrh    r3, [r5,#0x18]       @ tl
    strh    r0, [r5,#0x1a]       @ volume
    recalc_volume_ssg r0 r2

11:
    add     r0, r0, r3           @ volume += tl
    strh    r0, [r5,#0x34]       @ vol_out
#endif

0: @ EG_OFF
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
    ldr     r3, [lr, #0x60]
    ldr     r1, [lr, #0x20]
.endif
.ifc \row,r7
    ldr     r3, [lr, #0x64]
    ldr     r1, [lr, #0x24]
.endif
.ifc \row,r8
    ldr     r3, [lr, #0x68]
    ldr     r1, [lr, #0x28]
.endif
.ifc \row,r9
    ldr     r3, [lr, #0x6c]
    ldr     r1, [lr, #0x2c]
.endif
    @ N=old bit8 (mirror), C=old bit9 (sign); shifts below retain C.
    movs    \r, \r, lsl #23
    mvnmi   \r, \r
    mov     \r, \r, lsr #23
    mov     \r, \r, lsl #1
    ldrh    \r, [r3, \r]
    add     \row, \row, r1
    rsbcs   \r, \r, #0
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
    mov     r2, r7, lsr #16      @ 1ci
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
    mov     r0, r8, lsr #16      @ 1ci
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
    mov     r0, r8, lsr #16      @ 1ci
    lookup_tl r0 r8                 @ r0=output smp

0:
    @ SLOT4
    mov     r2, r9, lsr #16      @ 1ci
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
    mov     r0, r8, lsr #16      @ 1ci
    lookup_tl r0 r8                 @ r0=output smp

0:
    add     r0, r0, r10, asr #16

    @ SLOT4
    mov     r2, r9, lsr #16      @ 1ci
    lookup_tl r2 r9
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    mov     r2, r7, lsr #16      @ 1ci
    lookup_tl r2 r7
    add     r0, r0, r2           @ add to smp

2:
.endm


.macro upd_slot1_m

    cmp     r12, #0
    moveq   r0, #0
    addne   r0, r10, r10, lsl #16
    movne   r0, r0, asr #16
    movne   r0, r0, lsl r12

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
.global chan_render_loop_hot @ chan_rend_context *ct, int *buffer, int length

chan_render_loop_hot:
    ldr     r3, [r0, #0x4c]
    tst     r3, #1
    beq     chan_render_loop_hot_fallback
    and     r3, r3, #0x30
    cmp     r3, #0x10
    addeq   r1, r1, #4
    beq     side_ready
    cmp     r3, #0x20
    bne     chan_render_loop_hot_fallback
side_ready:
    stmfd   sp!, {r4-r11,lr}
    mov     lr,  r0
    mov     r4,  r2, lsl #8      @ no more 24 bits here
    ldr     r12, [lr, #0x4c]
    and     r12, r12, #0xf000
    mov     r12, r12, lsr #12
    ldr     r0,  [lr, #0x50]
    mov     r11, r1
    and     r0,  r0, #0xf7
    orr     r4,  r4, r0          @ (length<<8)|dac,upd_cnt[2],unused,algo[3]
    ldr     r8, [lr, #0x44]      @ eg_timer
    ldr     r9, [lr, #0x48]      @ eg_timer_add
    ldr     r10, [lr, #0x54]     @ op1_out

    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
    and r0,r4,#7
    PIC_XB(,r0, lsl #2)
    nop
    PIC_BT(crl_loop_0)
    PIC_BT(crl_loop_1)
    PIC_BT(crl_loop_2)
    PIC_BT(crl_loop_3)
    PIC_BT(crl_loop_4)
    PIC_BT(crl_loop_5)
    PIC_BT(crl_loop_6)
    PIC_BT(crl_loop_7)
    .pool
crl_loop_0:
    subs    r4, r4, #0x200
    bmi     crl_tail_0
    upd_slot1_m

    upd_algo0_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_0_a_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    @ One canonical carrier is already within[-8191,8191].
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_0_a_done:
crl_last_0:
    upd_slot1_m

    upd_algo0_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_0_b_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    @ One canonical carrier is already within[-8191,8191].
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_0_b_done:
    b       crl_loop_0
crl_tail_0:
    adds    r4, r4, #0x100
    bmi     crl_loop_end
    b       crl_last_0
crl_pair_0_a_zero:
    add     r11,r11,#8
    b       crl_pair_0_a_done
crl_pair_0_b_zero:
    add     r11,r11,#8
    b       crl_pair_0_b_done

    .pool
crl_loop_1:
    subs    r4, r4, #0x200
    bmi     crl_tail_1
    upd_slot1_m

    upd_algo1_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_1_a_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    @ One canonical carrier is already within[-8191,8191].
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_1_a_done:
crl_last_1:
    upd_slot1_m

    upd_algo1_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_1_b_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    @ One canonical carrier is already within[-8191,8191].
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_1_b_done:
    b       crl_loop_1
crl_tail_1:
    adds    r4, r4, #0x100
    bmi     crl_loop_end
    b       crl_last_1
crl_pair_1_a_zero:
    add     r11,r11,#8
    b       crl_pair_1_a_done
crl_pair_1_b_zero:
    add     r11,r11,#8
    b       crl_pair_1_b_done

    .pool
crl_loop_2:
    subs    r4, r4, #0x200
    bmi     crl_tail_2
    upd_slot1_m

    upd_algo2_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_2_a_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    @ One canonical carrier is already within[-8191,8191].
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_2_a_done:
crl_last_2:
    upd_slot1_m

    upd_algo2_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_2_b_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    @ One canonical carrier is already within[-8191,8191].
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_2_b_done:
    b       crl_loop_2
crl_tail_2:
    adds    r4, r4, #0x100
    bmi     crl_loop_end
    b       crl_last_2
crl_pair_2_a_zero:
    add     r11,r11,#8
    b       crl_pair_2_a_done
crl_pair_2_b_zero:
    add     r11,r11,#8
    b       crl_pair_2_b_done

    .pool
crl_loop_3:
    subs    r4, r4, #0x200
    bmi     crl_tail_3
    upd_slot1_m

    upd_algo3_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_3_a_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    @ One canonical carrier is already within[-8191,8191].
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_3_a_done:
crl_last_3:
    upd_slot1_m

    upd_algo3_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_3_b_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    @ One canonical carrier is already within[-8191,8191].
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_3_b_done:
    b       crl_loop_3
crl_tail_3:
    adds    r4, r4, #0x100
    bmi     crl_loop_end
    b       crl_last_3
crl_pair_3_a_zero:
    add     r11,r11,#8
    b       crl_pair_3_a_done
crl_pair_3_b_zero:
    add     r11,r11,#8
    b       crl_pair_3_b_done

    .pool
crl_loop_4:
    subs    r4, r4, #0x200
    bmi     crl_tail_4
    upd_slot1_m

    upd_algo4_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_4_a_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_4_a_done:
crl_last_4:
    upd_slot1_m

    upd_algo4_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_4_b_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_4_b_done:
    b       crl_loop_4
crl_tail_4:
    adds    r4, r4, #0x100
    bmi     crl_loop_end
    b       crl_last_4
crl_pair_4_a_zero:
    add     r11,r11,#8
    b       crl_pair_4_a_done
crl_pair_4_b_zero:
    add     r11,r11,#8
    b       crl_pair_4_b_done

    .pool
crl_loop_5:
    subs    r4, r4, #0x200
    bmi     crl_tail_5
    upd_slot1_m

    upd_algo5_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_5_a_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_5_a_done:
crl_last_5:
    upd_slot1_m

    upd_algo5_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_5_b_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_5_b_done:
    b       crl_loop_5
crl_tail_5:
    adds    r4, r4, #0x100
    bmi     crl_loop_end
    b       crl_last_5
crl_pair_5_a_zero:
    add     r11,r11,#8
    b       crl_pair_5_a_done
crl_pair_5_b_zero:
    add     r11,r11,#8
    b       crl_pair_5_b_done

    .pool
crl_loop_6:
    subs    r4, r4, #0x200
    bmi     crl_tail_6
    upd_slot1_m

    upd_algo6_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_6_a_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_6_a_done:
crl_last_6:
    upd_slot1_m

    upd_algo6_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_6_b_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_6_b_done:
    b       crl_loop_6
crl_tail_6:
    adds    r4, r4, #0x100
    bmi     crl_loop_end
    b       crl_last_6
crl_pair_6_a_zero:
    add     r11,r11,#8
    b       crl_pair_6_a_done
crl_pair_6_b_zero:
    add     r11,r11,#8
    b       crl_pair_6_b_done

    .pool
crl_loop_7:
    subs    r4, r4, #0x200
    bmi     crl_tail_7
    upd_slot1_m

    upd_algo7_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_7_a_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_7_a_done:
crl_last_7:
    upd_slot1_m

    upd_algo7_m
    @ Advance each original phase exactly once per native sample.
    @ Each lookup advanced its own phase after consuming it.

    tst     r0, r0
    beq     crl_pair_7_b_zero
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    ldr     r1, [r11]
    add     r1, r1, r0
    str     r1, [r11], #8
crl_pair_7_b_done:
    b       crl_loop_7
crl_tail_7:
    adds    r4, r4, #0x100
    bmi     crl_loop_end
    b       crl_last_7
crl_pair_7_a_zero:
    add     r11,r11,#8
    b       crl_pair_7_a_done
crl_pair_7_b_zero:
    add     r11,r11,#8
    b       crl_pair_7_b_done

    .pool
crl_loop_end:
    add     r3, lr, #0x10
    stmia   r3, {r6-r9}
    @ stable native EG timer is unchanged
    @ Original pack is unchanged; r12 holds only its feedback shift.
    str     r4,  [lr, #0x50]     @ was_update
    str     r10, [lr, #0x54]     @ op1_out
    ldmfd   sp!, {r4-r11,pc}

.pool

@ vim:filetype=armasm
