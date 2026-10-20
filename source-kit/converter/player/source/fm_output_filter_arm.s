/* Identical native clock/filter/clamp with an optional diagnostic hash.
 * Shared playback audits the complete mixed FM+PCM output, so hashing the
 * intermediate FM output a second time has no playback or verification use.
 * The unhashed variant leaves state[7] unchanged, including on zero frames. */
.syntax unified
.arm
.section .itcm,"ax",%progbits
.align 2
.macro output_channel last filter previous hashed
    ldr r0,=65279
    smull r1,r2,\filter,r0
    cmp r2,#0
    bge .Lrounded\@
    /* C signed division truncates towards zero, including negative values. */
    adds r1,r1,#0xff00
    adc r2,r2,#0
    adds r1,r1,#0xff
    adc r2,r2,#0
.Lrounded\@:
    mov r1,r1,lsr #16
    orr r1,r1,r2,lsl #16
    ldr r0,[sp]
    ldr r2,[r0,#\previous]
    mov r3,\last,lsl #1
    str r3,[r0,#\previous]
    sub r3,r3,r2
    add \filter,r3,r1
    mov r1,\filter,asr #15
    cmp r1,\filter,asr #31
    moveq r0,\filter
    movne r0,#32768
    subne r0,r0,#1
    eorne r0,r0,\filter,asr #31
    strh r0,[r6],#2
    .if \hashed
    mov r0,r0,lsl #16
    mov r0,r0,lsr #16
    eor r12,r12,r0
    ldr r1,=16777619
    mul r12,r1,r12
    .endif
.endm
.macro filter_function name hashed
.global \name
.type \name,%function
\name:
    push {r4-r11,lr}
    sub sp,sp,#12
    str r0,[sp]
    str r1,[sp,#4]
    mov r5,r1
    mov r6,r2
    mov r7,r3
    ldr r4,[r0,#0]
    ldr r8,[r0,#4]
    ldr r9,[r0,#8]
    ldr r10,[r0,#20]
    ldr r11,[r0,#24]
    .if \hashed
    ldr r12,[r0,#28]
    .endif
    cmp r7,#0
    beq 4f
1:
    ldr r0,=53693175
    add r4,r4,r0
    ldr r1,=32256000
    sub r4,r4,r1
    cmp r4,r1
    subhs r4,r4,r1
    addhs r5,r5,#8
    ldmia r5!,{r8,r9}

    output_channel r8,r10,12,\hashed
    output_channel r9,r11,16,\hashed
    subs r7,r7,#1
    bne 1b
4:
    ldr r0,[sp]
    str r4,[r0,#0]
    str r8,[r0,#4]
    str r9,[r0,#8]
    str r10,[r0,#20]
    str r11,[r0,#24]
    .if \hashed
    str r12,[r0,#28]
    .endif
    ldr r0,[sp,#4]
    sub r0,r5,r0
    mov r0,r0,lsr #3
    add sp,sp,#12
    pop {r4-r11,pc}
.size \name,.-\name
.endm
filter_function fm_output_filter_arm,1
filter_function fm_output_filter_arm_unhashed,0
.ltorg
