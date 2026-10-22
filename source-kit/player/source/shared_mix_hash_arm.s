/* Exact saturated16-bit FM+PCM mix and the existing sample-order FNV digest.
 * QADD on signed16.16 operands implements the same [-32768,32767] clamp.
 * Both channels and every original output sample remain in the hash.
 * Inputs need only halfword alignment; the PCM input is read-only. */
.syntax unified
.arm
.section .itcm,"ax",%progbits
.align 2
.global shared_mix_hash_arm
.type shared_mix_hash_arm,%function
.macro mix_sample
    ldrsh r4,[r0]
    ldrsh r5,[r1],#2
    mov r4,r4,lsl #16
    mov r5,r5,lsl #16
    qadd r4,r4,r5
    mov r4,r4,lsr #16
    strh r4,[r0],#2
.endm
.macro mix_one
    mix_sample
    eor r3,r3,r4
    mul r3,r6,r3
.endm
/* r0=FM/output, r1=PCM, r2=stereo frames, r3=running digest. */
shared_mix_hash_arm:
    cmp r2,#0
    moveq r0,r3
    bxeq lr
    push {r4-r6,lr}
    ldr r6,=16777619
1:
    mix_one
    mix_one
    subs r2,r2,#1
    bne 1b
    mov r0,r3
    pop {r4-r6,pc}
.size shared_mix_hash_arm,.-shared_mix_hash_arm
.pool
/* Production mixing has the same samples and saturation, without the audit
 * digest. Keep the observed entry above for explicit audio verification. */
.global shared_mix_arm
.type shared_mix_arm,%function
shared_mix_arm:
    cmp r2,#0
    bxeq lr
    push {r4-r5}
2:
    mix_sample
    mix_sample
    subs r2,r2,#1
    bne 2b
    pop {r4-r5}
    bx lr
.size shared_mix_arm,.-shared_mix_arm
.section .note.GNU-stack,"",%progbits
