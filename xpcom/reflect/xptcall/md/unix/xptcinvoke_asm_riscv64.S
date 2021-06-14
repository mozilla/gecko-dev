/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

    .set NGPREGS,8
    .set NFPREGS,8

    .text
    .globl  NS_InvokeByIndex
    .type   NS_InvokeByIndex, @function
/*
 * NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
 *                  uint32_t paramCount, nsXPTCVariant* params)
 */
NS_InvokeByIndex:
    .cfi_startproc
    addi    sp, sp, -32
    .cfi_adjust_cfa_offset 32
    sd      s0, 0(sp)
    sd      s1, 8(sp)
    sd      s2, 16(sp)
    sd      ra, 24(sp)

    mv      s0, a0 
    mv      s1, a1 
    mv      s2, sp 
    mv      a4, sp

    # 16-bytes alignment
    addiw   a0, a0, 1
    andi    a0, a0, -2
    slli    a0, a0, 3
    sub     sp, sp, a0

    addi    sp, sp, -8*(NGPREGS+NFPREGS)
    mv      a0, sp
    addi    a1, sp, 8*NGPREGS

    # extern "C" void invoke_copy_to_stack(uint64_t* gpregs, double* fpregs,
    #                                 uint32_t paramCount, nsXPTCVariant* s,
    #                                 uint64_t* d) {

    call    invoke_copy_to_stack
    addi    sp, sp, 8*(NGPREGS+NFPREGS)

    ld      a1, 8(s0)
    ld      a2, 16(s0)
    ld      a3, 24(s0)
    ld      a4, 32(s0)
    ld      a5, 40(s0)
    ld      a6, 48(s0)
    ld      a7, 56(s0)

    fld     fa0, 64(s0)
    fld     fa1, 72(s0)
    fld     fa2, 80(s0)
    fld     fa3, 88(s0)
    fld     fa4, 96(s0)
    fld     fa5, 104(s0)
    fld     fa6, 112(s0)
    fld     fa7, 120(s0)

    ld      s0, (s0)
    slli    s1, s1, 3
    add     s0, s0, s1
    ld      s0, [s0]
    call    s0

    mv      sp, s2
    ld      s0, 0(sp)
    ld      s1, 8(sp)
    ld      s2, 8(sp)
    ld      ra, 24(sp)
    addi    sp, sp, 32
    ret
    .cfi_endproc
    .size   NS_InvokeByIndex, .-NS_InvokeByIndex
    .section        .note.GNU-stack,"",@progbits
