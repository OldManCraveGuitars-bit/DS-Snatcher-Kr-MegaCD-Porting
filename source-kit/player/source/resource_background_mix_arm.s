.syntax unified
.arm
.section .itcm,"ax",%progbits
.align 2

/* Aligned byte codes, positive length divisible by4. Each byte is0..127.
 * A wins exactly when nonzero and either A has priority or B does not.
 * This is the original C plane/SAT choice, four independent pixels at once. */
.global resource_background_plane_mix_arm
.type resource_background_plane_mix_arm,%function
resource_background_plane_mix_arm:
    cmp r3,#0
    bxeq lr
    push {r4-r10,lr}
    ldr r4,=0x7f7f7f7f
    ldr r5,=0x40404040
1:
    ldr r6,[r0],#4
    ldr r7,[r1],#4
    add r8,r6,r4
    bic r8,r8,r4
    and r9,r6,r5
    and r10,r7,r5
    bic r10,r10,r9
    bic r8,r8,r10,lsl#1
    mov r9,r8,lsr#7
    rsb r9,r9,r8,lsl#1
    eor r6,r6,r7
    and r6,r6,r9
    eor r6,r6,r7
    str r6,[r2],#4
    subs r3,r3,#4
    bne 1b
    pop {r4-r10,pc}
.size resource_background_plane_mix_arm,.-resource_background_plane_mix_arm
.ltorg

.global resource_background_sprite_mix_arm
.type resource_background_sprite_mix_arm,%function
resource_background_sprite_mix_arm:
    cmp r3,#0
    bxeq lr
    ldr ip,[sp] /* fifth C argument: opaque palette bias0 or0x80808080 */
    push {r4-r11,lr}
    ldr r4,=0x7f7f7f7f
    ldr r5,=0x40404040
    ldr r11,=0x3f3f3f3f
2:
    ldr r6,[r0],#4
    ldr r7,[r1],#4
    add r8,r6,r4
    bic r8,r8,r4
    and r9,r6,r5
    and r10,r7,r5
    bic r10,r10,r9
    bic r8,r8,r10,lsl#1
    mov r9,r8,lsr#7
    rsb r9,r9,r8,lsl#1
    eor r6,r6,r7
    and r6,r6,r9
    eor r6,r6,r7
    and r6,r6,r11
    orr r6,r6,ip
    str r6,[r2],#4
    subs r3,r3,#4
    bne 2b
    pop {r4-r11,pc}
.size resource_background_sprite_mix_arm,.-resource_background_sprite_mix_arm
.ltorg
.section .note.GNU-stack,"",%progbits
