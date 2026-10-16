/* Exact ARM9 kernel for the existing52-bit PCM output clock and64-bit filter.
 * This source is installed only by the private candidate generator.
 * r0=state,r1=raw signed stereo,r2=output,r3=output frame count.
 * Returns the number of raw chip samples consumed. */
.syntax unified
.arm
.section .itcm,"ax",%progbits
.align 2
.global pcm_output_filter_arm
.type pcm_output_filter_arm,%function
pcm_output_filter_arm:
    push {r4-r11,lr}
    sub sp,sp,#4
    str r1,[sp]
    mov r4,r0
    mov r5,r1
    mov r6,r2
    mov r7,r3
    ldr r8,[r4,#8]
    ldr r9,[r4,#12]
    ldr r10,[r4,#0]
    ldr r11,[r4,#4]
    ldr r12,[r4,#304]
    cmp r7,#0
    beq 4f
1:
    cmp r9,#0x100000
    bhs 3f
2:
    ldrsh r0,[r5]
    ldr r2,[r4,#24]
    str r0,[r4,#24]
    sub r0,r0,r2
    ldrsh r1,[r5,#2]
    ldr r2,[r4,#28]
    str r1,[r4,#28]
    sub r1,r1,r2
    add r5,r5,#4
    /* Repeated chip samples add zero to both future delta slots. Keep the
     * previous samples, source-clock advance and complete output filter. */
    orrs r2,r0,r1
    beq 5f

    add r3,r12,#7
    and r3,r3,#31
    add r3,r4,r3,lsl #3
    mov r2,r9,lsr #5
    rsb r2,r2,#32768
    ldr lr,[r3,#32]
    mla lr,r0,r2,lr
    str lr,[r3,#32]
    ldr lr,[r3,#36]
    mla lr,r1,r2,lr
    str lr,[r3,#36]

    add r3,r12,#8
    and r3,r3,#31
    add r3,r4,r3,lsl #3
    mov r2,r9,lsr #5
    ldr lr,[r3,#32]
    mla lr,r0,r2,lr
    str lr,[r3,#32]
    ldr lr,[r3,#36]
    mla lr,r1,r2,lr
    str lr,[r3,#36]
5:
    adds r8,r8,r10
    adc r9,r9,r11
    cmp r9,#0x100000
    blo 2b
3:
    add lr,r4,r12,lsl #3
.macro output_channel offset delta
    ldr r0,[r4,#\offset]
    ldr r1,[r4,#(\offset+4)]
    /* Exact saturation of signed64-bit integrator>>15. Unsigned bias
     * detects the complete representable16-bit interval without narrowing. */
    adds r2,r0,#0x40000000
    adc r3,r1,#0
    cmp r3,#0
    cmpeq r2,#0x80000000
    movlo r2,r0,asr #15
    movhs r2,#32768
    subhs r2,r2,#1
    eorhs r2,r2,r1,asr #31
    ldr r3,[lr,#\delta]
    adds r0,r0,r3
    adc r1,r1,r3,asr #31
    mov r3,#0
    str r3,[lr,#\delta]
    subs r0,r0,r2,lsl #6
    sbc r1,r1,r2,asr #26
    str r0,[r4,#\offset]
    str r1,[r4,#(\offset+4)]
    strh r2,[r6],#2
.endm
    output_channel 288,32
    output_channel 296,36
    add r12,r12,#1
    and r12,r12,#31
    sub r9,r9,#0x100000
    subs r7,r7,#1
    bne 1b
4:
    str r8,[r4,#8]
    str r9,[r4,#12]
    str r12,[r4,#304]
    ldr r0,[sp]
    sub r0,r5,r0
    mov r0,r0,lsr #2
    add sp,sp,#4
    pop {r4-r11,pc}
.size pcm_output_filter_arm,.-pcm_output_filter_arm
