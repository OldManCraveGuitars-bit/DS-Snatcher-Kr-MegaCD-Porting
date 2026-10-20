/* Derived from the pinned PicoDrive YM loop (notaz/irixxxx).
 * Same MAME license as ym2612_hot.s. Shared state-only continuation for
 * discarded intermediate results; r0 is the local continuation address. */
.text
.align 2
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
.global fm_demand_state_0
fm_demand_state_0:
    mov r2,r10,asr #17
    add r2,r2,r7,lsr #16
    lookup_tl r2 r7
    str r2,[lr,#0x38]
    ldr r1,[lr,#0x28]
    ldr r2,[lr,#0x2c]
    add r8,r8,r1
    add r9,r9,r2
    add r11,r11,#8
    bx r0
.global fm_demand_state_1
fm_demand_state_1:
    mov r2,r7,lsr #16
    lookup_tl r2 r7
    add r2,r2,r10,asr #16
    str r2,[lr,#0x38]
    ldr r1,[lr,#0x28]
    ldr r2,[lr,#0x2c]
    add r8,r8,r1
    add r9,r9,r2
    add r11,r11,#8
    bx r0
.global fm_demand_state_2
fm_demand_state_2:
    mov r2,r7,lsr #16
    lookup_tl r2 r7
    str r2,[lr,#0x38]
    ldr r1,[lr,#0x28]
    ldr r2,[lr,#0x2c]
    add r8,r8,r1
    add r9,r9,r2
    add r11,r11,#8
    bx r0
.global fm_demand_state_3
.set fm_demand_state_3,fm_demand_state_0
.global fm_demand_state_4
fm_demand_state_4:
    ldr r1,[lr,#0x24]
    add r7,r7,r1
    ldr r1,[lr,#0x28]
    ldr r2,[lr,#0x2c]
    add r8,r8,r1
    add r9,r9,r2
    add r11,r11,#8
    bx r0
.global fm_demand_state_5
fm_demand_state_5:
    ldr r1,[lr,#0x24]
    add r7,r7,r1
    mov r1,r10,asr #16
    str r1,[lr,#0x38]
    ldr r1,[lr,#0x28]
    ldr r2,[lr,#0x2c]
    add r8,r8,r1
    add r9,r9,r2
    add r11,r11,#8
    bx r0
.global fm_demand_state_6
.set fm_demand_state_6,fm_demand_state_4
.global fm_demand_state_7
.set fm_demand_state_7,fm_demand_state_4
