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
    @ This operator is already proven due for the current exact EG counter.
    ldr     r2,[r7]
    mov     r0,r2,lsr #24
    mov     r3,r1,lsr r0
    and     r3,r3,#7
    add     r3,r3,r3,lsl #1
    mov     r3,r2,lsr r3
    and     r3,r3,#7
    ldr     r2,[r7,#4]
    mov     r2,r2,lsr #16
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
    ldrh r3,[r5,#0x18]
    strh r0,[r5,#0x1a]
    add     r0, r0, r3           @ volume += tl
    strh    r0, [r5,#0x34]       @ vol_out
    cmp     r0, #ENV_QUIET
    bic     r0, r0, #1
    movhs   r0, #ENV_QUIET
    ldr r3,=ym_linear_gain_lut
    ldr r3,[r3]
    ldrh r0,[r3,r0]
    ldr     r2,[r7,#8]
    str     r0,[r2]
    @ Register writes cannot occur inside this call. Only an EG state
    @ transition can change its packed rate before the next native sample.
    ldrb    r2,[r5,#0x17]
    ldr     r3,[r7,#4]
    cmp     r2,r3,lsr #16
    beq     0f
    cmp     r2,#0
    moveq   r0,#0
    moveq   r3,#4096
    subeq   r3,r3,#1
    beq     11f
    add     r3,r5,#0x1c
    ldr     r0,[r3,r2,lsl #2]
    str     r0,[r7]
    mov     r0,r0,lsr #24
    cmp     r0,#12
    mov     r3,#1
    movlo   r3,r3,lsl r0
    sublo   r3,r3,#1
    movhs   r3,#4096
    subhs   r3,r3,#1
    orr     r3,r3,r2,lsl #16
    str     r3,[r7,#4]
    b       12f
11:
    str     r0,[r7]
    str     r3,[r7,#4]
12:
    @ Any new nonzero low12period makes the next sample use due tests.
    movs    r0,r3,lsl #20
    ldrne   r0,=shared_eg_update
    strne   r0,[sp,#60]
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
    cmp     r3, #ENV_QUIET
    bic     r3, r3, #1
    addlo   r3, r8, r3, lsl #8
    movhs   r3, r9
    str     r3, [r7]
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
    mov \r,\r,lsl #22
    mov \r,\r,lsr #21
    ldrsh \r,[r5,\r]
 .ifc \row,r6
    ldr r1,[lr,#0x60]
 .endif
 .ifc \row,r7
    ldr r1,[lr,#0x64]
 .endif
 .ifc \row,r8
    ldr r1,[lr,#0x68]
 .endif
 .ifc \row,r9
    ldr r1,[lr,#0x6c]
 .endif
    smulbb \r,\r,r1
    mov \r,\r,asr #14
.endm


@ lr=context, r12=pack (stereo, ssg_enabled, disabled, lfo_enabled | pan_r, pan_l, ams[2] | AMmasks[4] | FB[4] | lfo_ampm[16])
@ r0-r2=scratch, r3=sin_tab, r5=scratch, r6-r7=vol_out[4], r10=op1_out
.macro upd_algo0_m

    @ SLOT3
    ldr r0,[lr,#0x38]
    mov r0,r0,asr #1
    add r0,r0,r8,lsr #16
    lookup_tl r0 r8                  @ r0=c2

0:
    @ SLOT4
    mov r0,r0,asr #1
    add r0,r0,r9,lsr #16
    lookup_tl r0 r9                  @ r0=output smp

1:
    @ SLOT2
    mov r2,r10,asr #17
    add r2,r2,r7,lsr #16
    lookup_tl r2 r7                  @ r2=mem

2:
    str     r2, [lr, #0x38] @ mem
.endm


.macro upd_algo1_m

    @ SLOT3
    ldr r0,[lr,#0x38]
    mov r0,r0,asr #1
    add r0,r0,r8,lsr #16
    lookup_tl r0 r8                 @ r0=c2

0:
    @ SLOT4
    mov r0,r0,asr #1
    add r0,r0,r9,lsr #16
    lookup_tl r0 r9                 @ r0=output smp

1:
    @ SLOT2
    mov r2,r7,lsr #16
    lookup_tl r2 r7                 @ r2=mem

2:
    add     r2, r2, r10, asr #16
    str     r2, [lr, #0x38]
.endm


.macro upd_algo2_m

    @ SLOT3
    ldr r0,[lr,#0x38]
    mov r0,r0,asr #1
    add r0,r0,r8,lsr #16
    lookup_tl r0 r8                 @ r0=c2

0:
    add     r0, r0, r10, asr #16

    @ SLOT4
    mov r0,r0,asr #1
    add r0,r0,r9,lsr #16
    lookup_tl r0 r9                 @ r0=output smp

1:
    @ SLOT2
    mov r2,r7,lsr #16
    lookup_tl r2 r7                 @ r2=mem

2:
    str     r2, [lr, #0x38] @ mem
.endm


.macro upd_algo3_m

    @ SLOT3
    ldr     r2, [lr, #0x38] @ mem (for future)
    mov r0,r8,lsr #16
    lookup_tl r0 r8                 @ r0=c2

0:
    add     r0, r0, r2

    @ SLOT4
    mov r0,r0,asr #1
    add r0,r0,r9,lsr #16
    lookup_tl r0 r9                 @ r0=output smp

1:
    @ SLOT2
    mov r2,r10,asr #17
    add r2,r2,r7,lsr #16
    lookup_tl r2 r7                 @ r2=mem

2:
    str     r2, [lr, #0x38]      @ mem
.endm


.macro upd_algo4_m

    @ SLOT3
    mov r0,r8,lsr #16
    lookup_tl r0 r8                 @ r0=c2

0:
    @ SLOT4
    mov r0,r0,asr #1
    add r0,r0,r9,lsr #16
    lookup_tl r0 r9                 @ r0=output smp

1:
    @ SLOT2
    mov r2,r10,asr #17
    add r2,r2,r7,lsr #16
    lookup_tl r2 r7
    add     r0, r0, r2            @ add to smp

2:
.endm


.macro upd_algo5_m

    @ SLOT3
    ldr r0,[lr,#0x38]
    mov r0,r0,asr #1
    add r0,r0,r8,lsr #16
    lookup_tl r0 r8                 @ r0=output smp

0:
    @ SLOT4
    mov r2,r10,asr #17
    add r2,r2,r9,lsr #16
    lookup_tl r2 r9
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    mov r2,r10,asr #17
    add r2,r2,r7,lsr #16
    lookup_tl r2 r7
    add     r0, r0, r2           @ add to smp

2:
    mov     r1, r10, asr #16
    str     r1, [lr, #0x38] @ mem
.endm


.macro upd_algo6_m

    @ SLOT3
    mov r0,r8,lsr #16
    lookup_tl r0 r8                 @ r0=output smp

0:
    @ SLOT4
    mov r2,r9,lsr #16
    lookup_tl r2 r9
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    mov r2,r10,asr #17
    add r2,r2,r7,lsr #16
    lookup_tl r2 r7
    add     r0, r0, r2           @ add to smp

2:
.endm


.macro upd_algo7_m

    @ SLOT3
    mov r0,r8,lsr #16
    lookup_tl r0 r8                 @ r0=output smp

0:
    add     r0, r0, r10, asr #16

    @ SLOT4
    mov r2,r9,lsr #16
    lookup_tl r2 r9
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    mov r2,r7,lsr #16
    lookup_tl r2 r7
    add     r0, r0, r2           @ add to smp

2:
.endm


.macro upd_slot1_m

    ands    r2, r12, #0xf000
    moveq   r0, #0
    movne   r2, r2, lsr #12
    addne   r0, r10, r10, lsl #16
    movne   r0, r0, asr #16
    movne   r0, r0, lsl r2

    add r0,r0,r6
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
    ldr r3,[r0,#0x0c]
    cmp r3,#0
    bne chan_render_loop_generic_fallback
    ldr r3,[r0,#0x4c]
    tst r3,#14
    bne chan_render_loop_generic_fallback
    ldr r3,[r0,#0x50]
    tst r3,#0x40
    bne chan_render_loop_generic_fallback
    stmfd   sp!, {r4-r11,lr}
    ldr r3,[r0,#0x0c]
    stmfd sp!,{r3}
    mov r4,r0
    mov r5,r1
    mov r6,r2
    sub sp,sp,#56            @ mask+four3word descriptors+padding; aligned C call
    mov r1,r6
    add r2,sp,#4
    bl fm_prepare_envelope_schedule
    cmn r0,#1
    beq envelope_schedule_fallback
    tst r0,#16
    adr r2,shared_eg_update_direct
    adreq r2,shared_eg_update
    str r2,[sp,#52]             @ after dispatch pushes: sp+60
    bic r0,r0,#16
    str r0,[sp]                 @ call-local potential-update mask
    mov r0,r4
    mov r1,r5
    mov r2,r6
    mov     lr,  r0
    mov     r4,  r2, lsl #8      @ no more 24 bits here
    ldr     r12, [lr, #0x4c]
    ldr     r0,  [lr, #0x50]
    mov     r11, r1
    and     r0,  r0, #0xf7
    orr     r4,  r4, r0          @ (length<<8)|dac,upd_cnt[2],unused,algo[3]
    ldr     r8, [lr, #0x44]      @ eg_timer
    ldr     r9, [lr, #0x48]      @ eg_timer_add
    ldr     r10, [lr, #0x54]     @ op1_out
    ldr     r5, [lr, #0x40]

    ldr r3,=ym_linear_gain_lut
    ldr r3,[r3]
    ldrh    r6, [r5, #0x34+SLOT_STRUCT_SIZE*0]
    cmp     r6, #ENV_QUIET
    bic     r6, r6, #1
    movhs   r6, #ENV_QUIET
    ldrh r6,[r3,r6]
    ldrh    r7, [r5, #0x34+SLOT_STRUCT_SIZE*2]
    cmp     r7, #ENV_QUIET
    bic     r7, r7, #1
    movhs   r7, #ENV_QUIET
    ldrh r7,[r3,r7]
    ldrh    r8, [r5, #0x34+SLOT_STRUCT_SIZE*1]
    cmp     r8, #ENV_QUIET
    bic     r8, r8, #1
    movhs   r8, #ENV_QUIET
    ldrh r8,[r3,r8]
    ldrh    r9, [r5, #0x34+SLOT_STRUCT_SIZE*3]
    cmp     r9, #ENV_QUIET
    bic     r9, r9, #1
    movhs   r9, #ENV_QUIET
    ldrh r9,[r3,r9]
    add     r0, lr, #0x60
    stmia   r0, {r6-r9}

    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
    ldr r5,=ym_linear_sine_lut
    ldr r5,[r5]
    and r0,r4,#7
    adr r3,algorithm_targets
    add r3,r3,r0,lsl #3
    ldmia r3,{r0-r1}
    str r1,[sp,#-4]!
    and r2,r12,#0x30
    adr r3,pan_targets
    ldr r2,[r3,r2,lsr #2]
    tst r12,#1
    adreq r2,pan_mono
    str r2,[sp,#-4]!
    bx r0
envelope_schedule_fallback:
    mov r0,r4
    mov r1,r5
    mov r2,r6
    add sp,sp,#60
    ldmfd sp!,{r4-r11,lr}
    b chan_render_loop_nolfo_demand_reference
pan_targets:
    .word pan_neither,pan_right,pan_left,pan_both
pan_neither:
    add r11,r11,#8
    bx r3
pan_right:
    ldr r1,[r11,#4]
    add r1,r1,r0
    str r1,[r11,#4]
    add r11,r11,#8
    bx r3
pan_left:
    ldr r1,[r11]
    add r1,r1,r0
    str r1,[r11],#8
    bx r3
pan_both:
    ldmia r11,{r1,r2}
    add r1,r1,r0
    add r2,r2,r0
    stmia r11!,{r1,r2}
    bx r3
pan_mono:
    ldr r1,[r11]
    add r1,r1,r0
    str r1,[r11],#4
    bx r3
algorithm_targets:
    .word fast_loop_0,after_eg_0
    .word fast_loop_1,after_eg_1
    .word fast_loop_2,after_eg_2
    .word fast_loop_3,after_eg_3
    .word fast_loop_4,after_eg_4
    .word fast_loop_5,after_eg_5
    .word fast_loop_6,after_eg_6
    .word fast_loop_7,after_eg_7
    .pool
shared_eg_schedule:
    and r0,r4,#0x30
    bic r4,r4,#0x30
    cmp r0,#0x20
    orrlo r4,r4,#0x70           @ two EG ticks, then countdown3
    subhs r0,r0,#0x10
    orrhs r4,r4,r0              @ one EG tick, countdown1 or2
    ldr pc,[sp,#60]
shared_eg_update_direct:

    ldr     r1, [lr, #0x3c]      @ eg_cnt
    add     r1, r1, #1
    cmp     r1, #4096
    movge   r1, #1
    str     r1, [lr, #0x3c]
    ldr r0,[sp,#8]
    b envelope_phase_save
shared_eg_update:

    ldr     r1, [lr, #0x3c]      @ eg_cnt
    add     r1, r1, #1
    cmp     r1, #4096
    movge   r1, #1
    str     r1, [lr, #0x3c]
    @ The per-call mask is zero when no envelope can change in this span.
    @ Still advance eg_cnt, but leave all phase registers and the LUT intact.
    ldr     r0, [sp, #8]
    cmp     r0, #0
    beq shared_eg_return
    @ Counter is already advanced. Select only operators due on this tick.
    @ Upper descriptor bits hold state; the exact counter has only12bits.
    add     r3,sp,#12
    ldr     r2,[r3,#4]
    tst     r1,r2
    moveq   r0,#1
    movne   r0,#0
    ldr     r2,[r3,#16]
    tst     r1,r2
    orreq   r0,r0,#2
    ldr     r2,[r3,#28]
    tst     r1,r2
    orreq   r0,r0,#4
    ldr     r2,[r3,#40]
    tst     r1,r2
    orreq   r0,r0,#8
    cmp     r0,#0
    beq shared_eg_return
envelope_phase_save:
    add     r3,lr,#0x10
    stmia   r3,{r6-r7}
    mov     r6,r0
    ldr     r5,[lr,#0x40]
    add     r7,sp,#12
eg_upd_loop:
    movs    r6,r6,lsr #1
    bcc     eg_first_unchanged
    update_eg_phase_slot
eg_first_unchanged:
    add     r7,r7,#12
    add     r5,r5,#SLOT_STRUCT_SIZE*2
    movs    r6,r6,lsr #1
    bcc     eg_second_unchanged
    update_eg_phase_slot
eg_second_unchanged:
    add     r7,r7,#12
    sub     r5,r5,#SLOT_STRUCT_SIZE
    cmp     r6,#0
    bne     eg_upd_loop

shared_eg_done:
    add r3,lr,#0x10
    ldmia r3,{r6-r7}
    ldr r5,=ym_linear_sine_lut
    ldr r5,[r5]
shared_eg_return:
    tst r4,#0x40
    bicne r4,r4,#0x40
    ldrne pc,[sp,#60]
    ldr pc,[sp,#4]
    .pool
fast_loop_0:
    subs r4,r4,#0x100
    bmi crl_loop_end
    b shared_eg_schedule
after_eg_0:
    upd_slot1_m
    upd_algo0_m
crl_algo_done_0:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    ldr     r1,[lr,#0x2c]
    add     r9, r9, r1

crl_smp_loop_end_0:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip_0
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    @ Canonical single carrier is already within the 14-bit bounds.
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    adr r3,fast_loop_0
    ldr pc,[sp]

ctl_sample_skip_0:
    and     r1, r12, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_0

    .pool
fast_loop_1:
    subs r4,r4,#0x100
    bmi crl_loop_end
    b shared_eg_schedule
after_eg_1:
    upd_slot1_m
    upd_algo1_m
crl_algo_done_1:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    ldr     r1,[lr,#0x2c]
    add     r9, r9, r1

crl_smp_loop_end_1:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip_1
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    @ Canonical single carrier is already within the 14-bit bounds.
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    adr r3,fast_loop_1
    ldr pc,[sp]

ctl_sample_skip_1:
    and     r1, r12, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_1

    .pool
fast_loop_2:
    subs r4,r4,#0x100
    bmi crl_loop_end
    b shared_eg_schedule
after_eg_2:
    upd_slot1_m
    upd_algo2_m
crl_algo_done_2:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    ldr     r1,[lr,#0x2c]
    add     r9, r9, r1

crl_smp_loop_end_2:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip_2
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    @ Canonical single carrier is already within the 14-bit bounds.
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    adr r3,fast_loop_2
    ldr pc,[sp]

ctl_sample_skip_2:
    and     r1, r12, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_2

    .pool
fast_loop_3:
    subs r4,r4,#0x100
    bmi crl_loop_end
    b shared_eg_schedule
after_eg_3:
    upd_slot1_m
    upd_algo3_m
crl_algo_done_3:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    ldr     r1,[lr,#0x2c]
    add     r9, r9, r1

crl_smp_loop_end_3:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip_3
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    @ Canonical single carrier is already within the 14-bit bounds.
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    adr r3,fast_loop_3
    ldr pc,[sp]

ctl_sample_skip_3:
    and     r1, r12, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_3

    .pool
fast_loop_4:
    subs r4,r4,#0x100
    bmi crl_loop_end
    b shared_eg_schedule
after_eg_4:
    upd_slot1_m
    upd_algo4_m
crl_algo_done_4:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    ldr     r1,[lr,#0x2c]
    add     r9, r9, r1

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
    adr r3,fast_loop_4
    ldr pc,[sp]

ctl_sample_skip_4:
    and     r1, r12, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_4

    .pool
fast_loop_5:
    subs r4,r4,#0x100
    bmi crl_loop_end
    b shared_eg_schedule
after_eg_5:
    upd_slot1_m
    upd_algo5_m
crl_algo_done_5:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    ldr     r1,[lr,#0x2c]
    add     r9, r9, r1

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
    adr r3,fast_loop_5
    ldr pc,[sp]

ctl_sample_skip_5:
    and     r1, r12, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_5

    .pool
fast_loop_6:
    subs r4,r4,#0x100
    bmi crl_loop_end
    b shared_eg_schedule
after_eg_6:
    upd_slot1_m
    upd_algo6_m
crl_algo_done_6:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    ldr     r1,[lr,#0x2c]
    add     r9, r9, r1

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
    adr r3,fast_loop_6
    ldr pc,[sp]

ctl_sample_skip_6:
    and     r1, r12, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_6

    .pool
fast_loop_7:
    subs r4,r4,#0x100
    bmi crl_loop_end
    b shared_eg_schedule
after_eg_7:
    upd_slot1_m
    upd_algo7_m
crl_algo_done_7:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    ldr     r1,[lr,#0x2c]
    add     r9, r9, r1

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
    adr r3,fast_loop_7
    ldr pc,[sp]

ctl_sample_skip_7:
    and     r1, r12, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       fast_loop_7

    .pool
crl_loop_end:
    add     r3, lr, #0x10
    stmia   r3, {r6-r9}
    @ Native EG timer is unchanged; r8 now holds an operator row.
    str     r12, [lr, #0x4c]     @ pack (for lfo_ampm)
    str     r4,  [lr, #0x50]     @ was_update
    str     r10, [lr, #0x54]     @ op1_out
    @ Restore the exact public context, including lookup row addresses.
    PIC_LDR(r2, r0, ym_tl_tab)
    ldr r2,[r2]
    PIC_LDR(r3, r0, ym_quiet_row)
    add r0,lr,#0x60
    ldr r5,[lr,#0x40]
    ldrh r6,[r5,#0x34+SLOT_STRUCT_SIZE*0]
    ldrh r7,[r5,#0x34+SLOT_STRUCT_SIZE*2]
    ldrh r8,[r5,#0x34+SLOT_STRUCT_SIZE*1]
    ldrh r9,[r5,#0x34+SLOT_STRUCT_SIZE*3]
.irp reg,r6,r7,r8,r9
    cmp \reg,#ENV_QUIET
    bic \reg,\reg,#1
    addlo \reg,r2,\reg,lsl #8
    movhs \reg,r3
.endr
    stmia r0,{r6-r9}
    ldr r0,[sp,#64]
    str r0,[lr,#0x0c]
    add sp,sp,#68
    ldmfd   sp!, {r4-r11,pc}

.pool

@ vim:filetype=armasm
