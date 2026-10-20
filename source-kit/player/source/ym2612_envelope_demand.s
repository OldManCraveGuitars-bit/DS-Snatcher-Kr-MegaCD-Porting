/* Same license as ym2612_nolfo_fast.s. All envelopes and SLOT1
 * have already advanced; retain only next-sample MEM dependencies. */
.text
.align 2
.macro lookup_tl r row
    @ Carry is phase bit9; the byte offset retains all nine half-wave bits.
    movs    \r, \r, lsl #23
    mov     \r, \r, lsr #22
    ldrh    \r, [r5, \r]
.ifc \row,r6
    ldr     r1, [lr, #0x60]
.endif
.ifc \row,r7
    ldr     r1, [lr, #0x64]
.endif
.ifc \row,r8
    ldr     r1, [lr, #0x68]
.endif
.ifc \row,r9
    ldr     r1, [lr, #0x6c]
.endif
    add     \r, \r, r1, lsl #2
    mov     r1, \r, lsr #8
    and     \r, \r, #0xff
    orr     \r, \r, #512
    mov     \r, \r, lsl #1
    ldrh    \r, [r5, \r]
    mov     \r, \r, lsr r1
    rsbcs   \r, \r, #0
.endm
.global fm_envelope_demand_0
fm_envelope_demand_0:
    mov r2,r10,asr #17
    add r2,r2,r7,lsr #16
    lookup_tl r2 r7
    str r2,[lr,#0x38]
    mov r3,r0
    mov r0,#0
    bx r3
.global fm_envelope_demand_1
fm_envelope_demand_1:
    mov r2,r7,lsr #16
    lookup_tl r2 r7
    add r2,r2,r10,asr #16
    str r2,[lr,#0x38]
    mov r3,r0
    mov r0,#0
    bx r3
.global fm_envelope_demand_2
fm_envelope_demand_2:
    mov r2,r7,lsr #16
    lookup_tl r2 r7
    str r2,[lr,#0x38]
    mov r3,r0
    mov r0,#0
    bx r3
.global fm_envelope_demand_3
.set fm_envelope_demand_3,fm_envelope_demand_0
.global fm_envelope_demand_4
fm_envelope_demand_4:
    mov r3,r0
    mov r0,#0
    bx r3
.global fm_envelope_demand_5
fm_envelope_demand_5:
    mov r1,r10,asr #16
    str r1,[lr,#0x38]
    mov r3,r0
    mov r0,#0
    bx r3
.global fm_envelope_demand_6
.set fm_envelope_demand_6,fm_envelope_demand_4
.global fm_envelope_demand_7
.set fm_envelope_demand_7,fm_envelope_demand_4
