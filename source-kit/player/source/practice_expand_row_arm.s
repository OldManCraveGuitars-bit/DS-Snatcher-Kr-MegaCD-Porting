.syntax unified
.arm
.section .itcm,"ax",%progbits
.align 2
/* Exactly224 input bytes ->256 opaque RGB555 halfwords. Source/output
 * four-byte aligned, palette halfword aligned. All64indices/high flags
 * keep their original meaning. Four groups use seven aligned input words,
 * never reading beyond byte223. No extra tables or resident caches. */
.global practice_expand_row_arm
.type practice_expand_row_arm,%function
practice_expand_row_arm:
    push {r4-r11,lr}
    ldr r12,=0x80008000
    mov r3,#8
1:
    ldr r4,[r0,#0]
    ldr r10,[r0,#4]
    and r5,r4,#0x3f
    add r5,r1,r5,lsl #1
    ldrh r5,[r5]
    orr r5,r5,r5,lsl #16
    and r6,r4,#0x3f00
    add r6,r1,r6,lsr #7
    ldrh r6,[r6]
    and r9,r4,#0x3f0000
    add r9,r1,r9,lsr #15
    ldrh r9,[r9]
    orr r6,r6,r9,lsl #16
    and r7,r4,#0x3f000000
    add r7,r1,r7,lsr #23
    ldrh r7,[r7]
    and r9,r10,#0x3f
    add r9,r1,r9,lsl #1
    ldrh r9,[r9]
    orr r7,r7,r9,lsl #16
    and r8,r10,#0x3f00
    add r8,r1,r8,lsr #7
    ldrh r8,[r8]
    and r9,r10,#0x3f0000
    add r9,r1,r9,lsr #15
    ldrh r9,[r9]
    orr r8,r8,r9,lsl #16
    orr r5,r5,r12
    orr r6,r6,r12
    orr r7,r7,r12
    orr r8,r8,r12
    stmia r2!,{r5-r8}
    ldr r11,[r0,#8]
    ldr r4,[r0,#12]
    and r5,r10,#0x3f000000
    add r5,r1,r5,lsr #23
    ldrh r5,[r5]
    orr r5,r5,r5,lsl #16
    and r6,r11,#0x3f
    add r6,r1,r6,lsl #1
    ldrh r6,[r6]
    and r9,r11,#0x3f00
    add r9,r1,r9,lsr #7
    ldrh r9,[r9]
    orr r6,r6,r9,lsl #16
    and r7,r11,#0x3f0000
    add r7,r1,r7,lsr #15
    ldrh r7,[r7]
    and r9,r11,#0x3f000000
    add r9,r1,r9,lsr #23
    ldrh r9,[r9]
    orr r7,r7,r9,lsl #16
    and r8,r4,#0x3f
    add r8,r1,r8,lsl #1
    ldrh r8,[r8]
    and r9,r4,#0x3f00
    add r9,r1,r9,lsr #7
    ldrh r9,[r9]
    orr r8,r8,r9,lsl #16
    orr r5,r5,r12
    orr r6,r6,r12
    orr r7,r7,r12
    orr r8,r8,r12
    stmia r2!,{r5-r8}
    ldr r10,[r0,#16]
    ldr r11,[r0,#20]
    and r5,r4,#0x3f0000
    add r5,r1,r5,lsr #15
    ldrh r5,[r5]
    orr r5,r5,r5,lsl #16
    and r6,r4,#0x3f000000
    add r6,r1,r6,lsr #23
    ldrh r6,[r6]
    and r9,r10,#0x3f
    add r9,r1,r9,lsl #1
    ldrh r9,[r9]
    orr r6,r6,r9,lsl #16
    and r7,r10,#0x3f00
    add r7,r1,r7,lsr #7
    ldrh r7,[r7]
    and r9,r10,#0x3f0000
    add r9,r1,r9,lsr #15
    ldrh r9,[r9]
    orr r7,r7,r9,lsl #16
    and r8,r10,#0x3f000000
    add r8,r1,r8,lsr #23
    ldrh r8,[r8]
    and r9,r11,#0x3f
    add r9,r1,r9,lsl #1
    ldrh r9,[r9]
    orr r8,r8,r9,lsl #16
    orr r5,r5,r12
    orr r6,r6,r12
    orr r7,r7,r12
    orr r8,r8,r12
    stmia r2!,{r5-r8}
    ldr r4,[r0,#24]
    and r5,r11,#0x3f00
    add r5,r1,r5,lsr #7
    ldrh r5,[r5]
    orr r5,r5,r5,lsl #16
    and r6,r11,#0x3f0000
    add r6,r1,r6,lsr #15
    ldrh r6,[r6]
    and r9,r11,#0x3f000000
    add r9,r1,r9,lsr #23
    ldrh r9,[r9]
    orr r6,r6,r9,lsl #16
    and r7,r4,#0x3f
    add r7,r1,r7,lsl #1
    ldrh r7,[r7]
    and r9,r4,#0x3f00
    add r9,r1,r9,lsr #7
    ldrh r9,[r9]
    orr r7,r7,r9,lsl #16
    and r8,r4,#0x3f0000
    add r8,r1,r8,lsr #15
    ldrh r8,[r8]
    and r9,r4,#0x3f000000
    add r9,r1,r9,lsr #23
    ldrh r9,[r9]
    orr r8,r8,r9,lsl #16
    orr r5,r5,r12
    orr r6,r6,r12
    orr r7,r7,r12
    orr r8,r8,r12
    stmia r2!,{r5-r8}
    add r0,r0,#28
    subs r3,r3,#1
    bne 1b
    pop {r4-r11,pc}
.size practice_expand_row_arm,.-practice_expand_row_arm
.ltorg
.section .note.GNU-stack,"",%progbits
