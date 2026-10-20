/* Original RF5C164 channel walk, with paired stereo accumulation.
 * r0=64KiB RAM, r1=32-bit position, r2=step, r3=loop address.
 * Stack arguments: int stereo_accumulator[], count, left gain, right gain.
 * Returns the advanced position. The 0xff loop marker never adds step,
 * including when the loop byte is another marker. No persistent state here. */
.syntax unified
.arm
.section .itcm,"ax",%progbits
.align 2
.global pcm_channel_mix_arm
.type pcm_channel_mix_arm,%function
pcm_channel_mix_arm:
    push {r4-r11,lr}
    ldr r4,[sp,#36]
    ldr r5,[sp,#40]
    ldr r6,[sp,#44]
    ldr r7,[sp,#48]
    mov r6,r6,lsl #11
    mov r7,r7,lsl #11
    cmp r5,#0
    beq 3f
    cmp r6,#0
    bne 4f
    cmp r7,#0
    beq 7f
    add r4,r4,#4
    mov r6,r7
    b 5f
4:
    cmp r7,#0
    beq 5f
1:
    mov r8,r1,lsr #11
    bic r8,r8,#0x1f0000
    ldrb r8,[r0,r8]
    cmp r8,#255
    addne r1,r1,r2
    moveq r1,r3,lsl #11
    ldrbeq r8,[r0,r3]
    cmp r8,#255
    beq 2f
    tst r8,#128
    andne r8,r8,#127
    rsbeq r8,r8,#0
    ldmia r4,{r11,r12}
    smlawb r11,r6,r8,r11
    smlawb r12,r7,r8,r12
    stmia r4!,{r11,r12}
    subs r5,r5,#1
    bne 1b
    b 3f
2:
    add r4,r4,#8
    subs r5,r5,#1
    bne 1b
3:
    mov r0,r1
    pop {r4-r11,pc}
/* One nonzero pan: same signed multiply/rounding and same channel walk.
 * The other accumulator is left untouched because its exact addend is0. */
5:
    mov r8,r1,lsr #11
    bic r8,r8,#0x1f0000
    ldrb r8,[r0,r8]
    cmp r8,#255
    addne r1,r1,r2
    moveq r1,r3,lsl #11
    ldrbeq r8,[r0,r3]
    cmp r8,#255
    beq 6f
    tst r8,#128
    andne r8,r8,#127
    rsbeq r8,r8,#0
    ldr r11,[r4]
    smlawb r11,r6,r8,r11
    str r11,[r4]
6:
    add r4,r4,#8
    subs r5,r5,#1
    bne 5b
    b 3b
/* Zero gain still walks every original RAM byte and loop marker. */
7:
    mov r8,r1,lsr #11
    bic r8,r8,#0x1f0000
    ldrb r8,[r0,r8]
    cmp r8,#255
    addne r1,r1,r2
    moveq r1,r3,lsl #11
    subs r5,r5,#1
    bne 7b
    b 3b
.size pcm_channel_mix_arm,.-pcm_channel_mix_arm
/* Equal channels share RAM walking; round each side before multiplying
 * the number of equal source channels. All sums fit signed32 (8*15181).
 * Additional stack argument: original-channel multiplicity, range2..8. */
.align 2
.global pcm_channel_group_arm
.type pcm_channel_group_arm,%function
pcm_channel_group_arm:
    push {r4-r11,lr}
    ldr r4,[sp,#36]
    ldr r5,[sp,#40]
    ldr r6,[sp,#44]
    ldr r7,[sp,#48]
    ldr lr,[sp,#52]
    cmp r5,#0
    beq 3f
1:
    mov r8,r1,lsr #11
    bic r8,r8,#0x1f0000
    ldrb r8,[r0,r8]
    cmp r8,#255
    addne r1,r1,r2
    moveq r1,r3,lsl #11
    ldrbeq r8,[r0,r3]
    cmp r8,#255
    beq 2f
    tst r8,#128
    andne r8,r8,#127
    rsbeq r8,r8,#0
    smulbb r9,r8,r6
    smulbb r10,r8,r7
    mov r9,r9,asr #5
    mov r10,r10,asr #5
    smulbb r9,lr,r9
    smulbb r10,lr,r10
    ldmia r4,{r11,r12}
    add r11,r11,r9
    add r12,r12,r10
    stmia r4!,{r11,r12}
    subs r5,r5,#1
    bne 1b
    b 3f
2:
    add r4,r4,#8
    subs r5,r5,#1
    bne 1b
3:
    mov r0,r1
    pop {r4-r11,pc}
.size pcm_channel_group_arm,.-pcm_channel_group_arm
.section .note.GNU-stack,"",%progbits
