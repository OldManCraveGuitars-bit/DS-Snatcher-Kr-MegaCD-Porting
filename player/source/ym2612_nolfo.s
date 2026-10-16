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
    cmp     r0, #ENV_QUIET
    bic     r0, r0, #1
    addlo   r0, r8, r0, lsl #8
    movhs   r0, r9
    str     r0, [r7]
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
    tst     \r, #0x100
    eorne   \r, \r, #0xff
    tst     \r, #0x200
    and     \r, \r, #0xff
    mov     \r, \r, lsl #1
.ifc \row,r6
    ldr     r3, [lr, #0x60]
.endif
.ifc \row,r7
    ldr     r3, [lr, #0x64]
.endif
.ifc \row,r8
    ldr     r3, [lr, #0x68]
.endif
.ifc \row,r9
    ldr     r3, [lr, #0x6c]
.endif
    ldrh    \r, [r3, \r]
    rsbne   \r, \r, #0
.endm


@ lr=context, r12=pack (stereo, ssg_enabled, disabled, lfo_enabled | pan_r, pan_l, ams[2] | AMmasks[4] | FB[4] | lfo_ampm[16])
@ r0-r2=scratch, r3=sin_tab, r5=scratch, r6-r7=vol_out[4], r10=op1_out
.macro upd_algo0_m

    @ SLOT3
    mov     r2, r8
    ldr     r0, [lr, #0x38] @ mem (signed)
    mov     r2, r2, lsr #16
    add     r0, r2, r0, asr #1
    lookup_tl r0 r8                  @ r0=c2

0:
    @ SLOT4
    mov     r2, r9
    mov     r0, r0, asr #1
    add     r0, r0, r2, lsr #16
    lookup_tl r0 r9                  @ r0=output smp

1:
    @ SLOT2
    mov     r2, r7       @ 1ci
    mov     r5, r10, asr #17
    add     r2, r5, r2, lsr #16
    lookup_tl r2 r7                  @ r2=mem

2:
    str     r2, [lr, #0x38] @ mem
.endm


.macro upd_algo1_m

    @ SLOT3
    mov     r2, r8
    ldr     r0, [lr, #0x38] @ mem (signed)
    mov     r2, r2, lsr #16
    add     r0, r2, r0, asr #1
    lookup_tl r0 r8                 @ r0=c2

0:
    @ SLOT4
    mov     r2, r9
    mov     r0, r0, asr #1
    add     r0, r0, r2, lsr #16
    lookup_tl r0 r9                 @ r0=output smp

1:
    @ SLOT2
    mov     r2, r7      @ 1ci
    mov     r2, r2, lsr #16
    lookup_tl r2 r7                 @ r2=mem

2:
    add     r2, r2, r10, asr #16
    str     r2, [lr, #0x38]
.endm


.macro upd_algo2_m

    @ SLOT3
    mov     r2, r8
    ldr     r0, [lr, #0x38] @ mem (signed)
    mov     r2, r2, lsr #16
    add     r0, r2, r0, asr #1
    lookup_tl r0 r8                 @ r0=c2

0:
    add     r0, r0, r10, asr #16

    @ SLOT4
    mov     r2, r9
    mov     r0, r0, asr #1
    add     r0, r0, r2, lsr #16
    lookup_tl r0 r9                 @ r0=output smp

1:
    @ SLOT2
    mov     r2, r7
    mov     r2, r2, lsr #16      @ 1ci
    lookup_tl r2 r7                 @ r2=mem

2:
    str     r2, [lr, #0x38] @ mem
.endm


.macro upd_algo3_m

    @ SLOT3
    ldr     r2, [lr, #0x38] @ mem (for future)
    mov     r0, #0
    mov     r0, r8      @ phase3
    mov     r0, r0, lsr #16
    lookup_tl r0 r8                 @ r0=c2

0:
    add     r0, r0, r2

    @ SLOT4
    mov     r2, r9
    mov     r0, r0, asr #1
    add     r0, r0, r2, lsr #16
    lookup_tl r0 r9                 @ r0=output smp

1:
    @ SLOT2
    mov     r2, r7      @ phase2
    mov     r5, r10, asr #17
    add     r2, r5, r2, lsr #16
    lookup_tl r2 r7                 @ r2=mem

2:
    str     r2, [lr, #0x38]      @ mem
.endm


.macro upd_algo4_m

    @ SLOT3
    mov     r0, r8
    mov     r0, r0, lsr #16      @ 1ci
    lookup_tl r0 r8                 @ r0=c2

0:
    @ SLOT4
    mov     r2, r9
    mov     r0, r0, asr #1
    add     r0, r0, r2, lsr #16
    lookup_tl r0 r9                 @ r0=output smp

1:
    @ SLOT2
    mov     r2, r7
    mov     r5, r10, asr #17
    add     r2, r5, r2, lsr #16
    lookup_tl r2 r7
    add     r0, r0, r2            @ add to smp

2:
.endm


.macro upd_algo5_m

    @ SLOT3
    mov     r2, r8
    ldr     r0, [lr, #0x38] @ mem (signed)
    mov     r2, r2, lsr #16
    add     r0, r2, r0, asr #1
    lookup_tl r0 r8                 @ r0=output smp

0:
    @ SLOT4
    mov     r2, r9
    mov     r5, r10, asr #17
    add     r2, r5, r2, lsr #16
    lookup_tl r2 r9
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    mov     r2, r7
    mov     r5, r10, asr #17
    add     r2, r5, r2, lsr #16
    lookup_tl r2 r7
    add     r0, r0, r2           @ add to smp

2:
    mov     r1, r10, asr #16
    str     r1, [lr, #0x38] @ mem
.endm


.macro upd_algo6_m

    @ SLOT3
    mov     r0, r8
    mov     r0, r0, lsr #16      @ 1ci
    lookup_tl r0 r8                 @ r0=output smp

0:
    @ SLOT4
    mov     r2, r9
    mov     r2, r2, lsr #16      @ 1ci
    lookup_tl r2 r9
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    mov     r2, r7
    mov     r5, r10, asr #17
    add     r2, r5, r2, lsr #16
    lookup_tl r2 r7
    add     r0, r0, r2           @ add to smp

2:
.endm


.macro upd_algo7_m

    @ SLOT3
    mov     r0, r8
    mov     r0, r0, lsr #16      @ 1ci
    lookup_tl r0 r8                 @ r0=output smp

0:
    add     r0, r0, r10, asr #16

    @ SLOT4
    mov     r2, r9
    mov     r2, r2, lsr #16      @ 1ci
    lookup_tl r2 r9
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    mov     r2, r7
    mov     r2, r2, lsr #16      @ 1ci
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

    mov     r2, r6     @ phase1
    add     r0, r0, r2
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
.global chan_render_loop_nolfo_fallback @ chan_rend_context *ct, int *buffer, int length

chan_render_loop_nolfo_fallback:
    stmfd   sp!, {r4-r11,lr}
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
    PIC_LDR(r2, r0, ym_tl_tab)
    ldr r2, [r2]
    PIC_LDR(r3, r0, ym_quiet_row)
    ldrh    r6, [r5, #0x34+SLOT_STRUCT_SIZE*0]
    cmp     r6, #ENV_QUIET
    bic     r6, r6, #1
    addlo   r6, r2, r6, lsl #8
    movhs   r6, r3
    ldrh    r7, [r5, #0x34+SLOT_STRUCT_SIZE*2]
    cmp     r7, #ENV_QUIET
    bic     r7, r7, #1
    addlo   r7, r2, r7, lsl #8
    movhs   r7, r3
    ldrh    r8, [r5, #0x34+SLOT_STRUCT_SIZE*1]
    cmp     r8, #ENV_QUIET
    bic     r8, r8, #1
    addlo   r8, r2, r8, lsl #8
    movhs   r8, r3
    ldrh    r9, [r5, #0x34+SLOT_STRUCT_SIZE*3]
    cmp     r9, #ENV_QUIET
    bic     r9, r9, #1
    addlo   r9, r2, r9, lsl #8
    movhs   r9, r3
    add     r0, lr, #0x60
    stmia   r0, {r6-r9}

    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
crl_loop:
    subs    r4, r4, #0x100
    bmi     crl_loop_end

crl_eg_loop:
#if defined(SSG_EG)
    tst     r12, #0x02           @ ssg_enabled?
    beq     ssg_done

    @ -- SSG --
    add     r0, lr, #0x10
    stmia   r0, {r6-r9}
    ldr     r5, [lr, #0x40]
    PIC_LDR(r8, r0, ym_tl_tab)
    ldr r8, [r8]
    PIC_LDR(r9, r0, ym_quiet_row)
    add     r7, lr, #0x60
    mov     r6, #4
ssg_upd_loop:
    @ use lr as a pointer to the slot phases stored in the context
    update_ssg_eg
    add     r7, r7, #4
#if 0
    tst     r6, #1
    addeq   lr, lr, #2*4
    subne   lr, lr, #4
    subs    r6, r6, #1
    addeq   lr, lr, #4
    addne   r5, r5, #SLOT_STRUCT_SIZE
#else
    add     lr, lr, #4
    add     r5, r5, #SLOT_STRUCT_SIZE*2
    update_ssg_eg
    add     r7, r7, #4
    subs    r6, r6, #2
    addne   lr, lr, #4
    subne   r5, r5, #SLOT_STRUCT_SIZE
#endif
    bne     ssg_upd_loop
    sub     lr, lr, #4*3
    sub     r5, r5, #SLOT_STRUCT_SIZE*3

ssg_done:
#endif

    @ -- EG --
    tst     r4, #0x30
    subne   r4, r4, #0x10
    tst     r4, #0x30
    bne     eg_done
    orr     r4, r4, #0x30

    tst     r12, #2
    addeq   r0, lr, #0x10
    stmeqia r0, {r6-r9}
    ldr     r1, [lr, #0x3c]      @ eg_cnt
    add     r1, r1, #1
    cmp     r1, #4096
    movge   r1, #1
    str     r1, [lr, #0x3c]
    ldr     r0, [lr, #0x58]
    cmp     r0, #0
    bne     eg_done

    ldr     r5, [lr, #0x40]
    PIC_LDR(r8, r0, ym_tl_tab)
    ldr r8, [r8]
    PIC_LDR(r9, r0, ym_quiet_row)
    add     r7, lr, #0x60
    mov     r6, #4
eg_upd_loop:
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
    bne     eg_upd_loop
    sub     r5, r5, #SLOT_STRUCT_SIZE*3

    b       restore_phase_registers
eg_done:
    tst     r12, #2
    beq     eg_loop_done
restore_phase_registers:
    add     r3, lr, #0x10
    ldmia   r3, {r6-r9}
eg_loop_done:
    @ -- disabled? --
    mov     r0, #0
    tst     r12, #0x4
    bne     crl_algo_done


    @ Phase accumulators remain in r6-r9.

    @ lr=context, r12=pack (stereo, ssg_enabled, disabled, lfo_enabled | pan_r, pan_l, ams[2] | AMmasks[4] | FB[4] | lfo_ampm[16])
    @ r0-r2=scratch, r3=tl_tab, r5=scratch, r6-r7=vol_out[4], r10=op1_out

    @ -- SLOT1 --
    upd_slot1_m

    @ -- SLOT2+ --
    and     r0, r4, #7
    PIC_XB(,r0, lsl #2)
    nop
    PIC_BT(crl_algo0)
    PIC_BT(crl_algo1)
    PIC_BT(crl_algo2)
    PIC_BT(crl_algo3)
    PIC_BT(crl_algo4)
    PIC_BT(crl_algo5)
    PIC_BT(crl_algo6)
    PIC_BT(crl_algo7)
    .pool

crl_algo0:
    upd_algo0_m
    b       crl_algo_done
    .pool

crl_algo1:
    upd_algo1_m
    b       crl_algo_done
    .pool

crl_algo2:
    upd_algo2_m
    b       crl_algo_done
    .pool

crl_algo3:
    upd_algo3_m
    b       crl_algo_done
    .pool

crl_algo4:
    upd_algo4_m
    b       crl_algo_done
    .pool

crl_algo5:
    upd_algo5_m
    b       crl_algo_done
    .pool

crl_algo6:
    upd_algo6_m
    b       crl_algo_done
    .pool

crl_algo7:
    upd_algo7_m


crl_algo_done:
    @ Advance each original phase exactly once per native sample.
    add     r3, lr, #0x20
    ldmia   r3, {r1-r3,r5}
    add     r6, r6, r1
    add     r7, r7, r2
    add     r8, r8, r3
    add     r9, r9, r5

crl_smp_loop_end:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip
    orr     r4, r4, #8              @ have_output
    mov     r1, r0, lsr #31         @ clip (saturate) sample to 14 bit
    cmn     r1, r0, asr #13
    subne   r0, r1, #0x80000001
    movne   r0, r0, asr #18
    tst     r4, #0x80               @ dac?
    bicne   r0, r0, #0x1f
    tst     r4, r1, lsl #7          @ (sample < 0) && dac?
    subne   r0, r0, #7<<5
    tst     r12, #1
    beq     ctl_sample_mono

    tst     r12, #0x20              @ L
    ldrne   r1, [r11]
    addeq   r11, r11, #4
    addne   r1, r0, r1
    strne   r1, [r11], #4
    tst     r12, #0x10              @ R
    ldrne   r1, [r11]
    addeq   r11, r11, #4
    addne   r1, r0, r1
    strne   r1, [r11], #4
    b       crl_loop

ctl_sample_mono:
    ldr     r1, [r11]
    add     r1, r0, r1
    str     r1, [r11], #4
    b       crl_loop

ctl_sample_skip:
    and     r1, r12, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2
    b       crl_loop

crl_loop_end:
    add     r3, lr, #0x10
    stmia   r3, {r6-r9}
    @ Native EG timer is unchanged; r8 now holds an operator row.
    str     r12, [lr, #0x4c]     @ pack (for lfo_ampm)
    str     r4,  [lr, #0x50]     @ was_update
    str     r10, [lr, #0x54]     @ op1_out
    ldmfd   sp!, {r4-r11,pc}

.pool

@ vim:filetype=armasm
