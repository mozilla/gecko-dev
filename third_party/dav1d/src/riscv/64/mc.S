/******************************************************************************
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2024, Nathan Egge, Niklas Haas, Bogdan Gligorijevic
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include "src/riscv/asm.S"

function blend_vl256_8bpc_rvv, export=1, ext=zbb
  ctz t0, a3
  addi t0, t0, 0xc3
  j L(blend_epilog)
endfunc

function blend_8bpc_rvv, export=1, ext="v,zbb"
  ctz t0, a3
  addi t0, t0, 0xc4
L(blend_epilog):
  csrw vxrm, zero
  andi t0, t0, 0xc7
  vsetvl zero, a3, t0
  li t1, 64
1:
  addi a4, a4, -2
  vle8.v v4, (a2)
  add a2, a2, a3
  vle8.v v6, (a2)
  add a2, a2, a3
  vle8.v v8, (a5)
  add a5, a5, a3
  vle8.v v10, (a5)
  add a5, a5, a3
  vle8.v v0, (a0)
  add t0, a0, a1
  vle8.v v2, (t0)
  vwmulu.vv v16, v4, v8
  vwmulu.vv v20, v6, v10
  vrsub.vx v8, v8, t1
  vrsub.vx v10, v10, t1
  vwmaccu.vv v16, v0, v8
  vwmaccu.vv v20, v2, v10
  vnclipu.wi v0, v16, 6
  vnclipu.wi v2, v20, 6
  vse8.v v0, (a0)
  vse8.v v2, (t0)
  add a0, t0, a1
  bnez a4, 1b
  ret
endfunc

function blend_h_vl256_8bpc_rvv, export=1, ext=zbb
  srai t0, a3, 2
  li t2, 64
  ctz t0, t0
  addi t0, t0, 0xc5
  j L(blend_h_epilog)
endfunc

function blend_h_8bpc_rvv, export=1, ext="v,zbb"
  li t2, 64
  bgt a3, t2, 128f
  ctz t0, a3
  addi t0, t0, 0xc4
L(blend_h_epilog):
  csrw vxrm, zero
  andi t0, t0, 0xc7
  vsetvl zero, a3, t0
  la t1, dav1d_obmc_masks
  srai t0, a4, 2
  add t1, t1, a4
  sub a4, a4, t0
0:
  mv t5, ra
1:
  addi a4, a4, -2
  lbu t3, (t1)
  addi t1, t1, 1
  lbu t4, (t1)
  addi t1, t1, 1
  vle8.v v8, (a2)
  add a2, a2, a3
  vle8.v v12, (a2)
  add a2, a2, a3
  vle8.v v0, (a0)
  add t0, a0, a1
  vle8.v v4, (t0)
  vwmulu.vx v16, v8, t3
  vwmulu.vx v24, v12, t4
  sub t3, t2, t3
  sub t4, t2, t4
  vwmaccu.vx v16, t3, v0
  vwmaccu.vx v24, t4, v4
  vnclipu.wi v0, v16, 6
  vnclipu.wi v4, v24, 6
  vse8.v v0, (a0)
  vse8.v v4, (t0)
  add a0, t0, a1
  bgtz a4, 1b
  jr t5
128:
  csrw vxrm, zero
  vsetvli zero, t2, e8, m4, ta, ma
  la t1, dav1d_obmc_masks
  srai t0, a4, 2
  add t1, t1, a4
  sub a4, a4, t0
  mv a5, a0
  mv a6, a2
  mv a7, a4
  jal t5, 1b
  add t1, t1, a4
  add a0, a5, t2
  add a2, a6, t2
  mv a4, a7
  sub t1, t1, a4
  j 0b
endfunc

function blend_v_vl256_8bpc_rvv, export=1, ext=zbb
  srai t0, a3, 2
  ctz t0, t0
  addi t0, t0, 0xc5
  j L(blend_v_epilog)
endfunc

function blend_v_8bpc_rvv, export=1, ext="v,zbb"
  ctz t0, a3
  addi t0, t0, 0xc4
L(blend_v_epilog):
  andi t0, t0, 0xc7
  srai t1, a3, 2
  sub t1, a3, t1
  vsetvl zero, t1, t0
  csrw vxrm, zero
  la t1, dav1d_obmc_masks
  add t1, t1, a3
  vle8.v v8, (t1)
  li t0, 64
  vrsub.vx v10, v8, t0
1:
  addi a4, a4, -2
  vle8.v v4, (a2)
  add a2, a2, a3
  vle8.v v6, (a2)
  add a2, a2, a3
  vle8.v v0, (a0)
  add t0, a0, a1
  vle8.v v2, (t0)
  vwmulu.vv v12, v4, v8
  vwmulu.vv v16, v6, v8
  vwmaccu.vv v12, v0, v10
  vwmaccu.vv v16, v2, v10
  vnclipu.wi v0, v12, 6
  vnclipu.wi v2, v16, 6
  vse8.v v0, (a0)
  vse8.v v2, (t0)
  add a0, t0, a1
  bnez a4, 1b
  ret
endfunc

.macro avg va, vb, vm
    vadd.vv \va, \va, \vb
.endm

.macro w_avg va, vb, vm
    vwmul.vx v24, \va, a6
    vwmacc.vx v24, a7, \vb
    vnclip.wi \va, v24, 8
.endm

.macro mask va, vb, vm
    vwmul.vv v24, \va, \vm
    vrsub.vx \vm, \vm, a7
    vwmacc.vv v24, \vb, \vm
    vnclip.wi \va, v24, 10
.endm

.macro bidir_fn type, shift
function \type\()_8bpc_rvv, export=1, ext="v,zba,zbb"
.ifc \type, w_avg
    li a7, 16
    sub a7, a7, a6
.endif
.ifc \type, mask
    li a7, 64
.endif
    li t0, 4
    csrw vxrm, zero
    beq t0, a4, 4f
    csrr t0, vlenb
    ctz t1, a4
    ctz t0, t0
    li t2, 1
    sub t0, t1, t0
    li t4, -3
    bgt t0, t2, 2f
    max t0, t0, t4
    andi t1, t0, 0x7
    addi t0, t1, 1 # may overflow into E16 bit
    ori t0, t0, MA | TA | E16
    ori t1, t1, MA | TA | E8
1:
    addi a5, a5, -4
.rept 2
    vsetvl zero, a4, t0
    sh1add t3, a4, a2
    vle16.v v0, (a2)
    sh1add a2, a4, t3
    vle16.v v4, (t3)
    sh1add t3, a4, a3
    vle16.v v8, (a3)
    sh1add a3, a4, t3
    vle16.v v12, (t3)
.ifc \type, mask
    add t3, a4, a6
    vle8.v v24, (a6)
    add a6, a4, t3
    vle8.v v26, (t3)
    vzext.vf2 v16, v24
    vzext.vf2 v20, v26
.endif
    \type v0, v8, v16
    \type v4, v12, v20
    vmax.vx v8, v0, zero
    vmax.vx v12, v4, zero
    vsetvl zero, zero, t1
    vnclipu.wi v0, v8,  \shift
    vnclipu.wi v2, v12, \shift
    add t3, a1, a0
    vse8.v v0, (a0)
    add a0, a1, t3
    vse8.v v2, (t3)
.endr
    bnez a5, 1b
    ret
2:
    mv t0, a0
    neg t4, a4
    add a0, a1, a0
    addi a5, a5, -1
20:
    vsetvli t2, a4, e16, m4, ta, ma
    sh1add t4, t2, t4
    sh1add t3, t2, a2
    vle16.v v0, (a2)
    sh1add a2, t2, t3
    vle16.v v4, (t3)
    sh1add t3, t2, a3
    vle16.v v8, (a3)
    sh1add a3, t2, t3
    vle16.v v12, (t3)
.ifc \type, mask
    add t3, t2, a6
    vle8.v v24, (a6)
    add a6, t2, t3
    vle8.v v26, (t3)
    vzext.vf2 v16, v24
    vzext.vf2 v20, v26
.endif
    \type v0, v8, v16
    \type v4, v12, v20
    vmax.vx v8, v0, zero
    vmax.vx v12, v4, zero
    vsetvli zero, zero, e8, m2, ta, ma
    vnclipu.wi v0, v8,  \shift
    vnclipu.wi v2, v12, \shift
    add t3, t2, t0
    vse8.v v0, (t0)
    add t0, t2, t3
    vse8.v v2, (t3)
    bnez t4, 20b
    bnez a5, 2b
    ret
4:
    slli t0, a5, 2
    vsetvli t1, t0, e16, m4, ta, ma
    vle16.v v0, (a2)
    sh1add a2, t1, a2
    vle16.v v4, (a3)
    sh1add a3, t1, a3
.ifc \type, mask
    vle8.v v16, (a6)
    add a6, t1, a6
    vzext.vf2 v8, v16
.endif
    \type v0, v4, v8
    vmax.vx v8, v0, zero
    vsetvli zero, zero, e8, m2, ta, ma
    vnclipu.wi v0, v8, \shift
    vsetvli t1, a5, e32, m2, ta, ma
    vsse32.v v0, (a0), a1
    ctz t0, t1
    sub a5, a5, t1
    sll t0, a1, t0
    add a0, t0, a0
    bnez a5, 4b
    ret
endfunc
.endm

bidir_fn avg,   5
bidir_fn w_avg, 0
bidir_fn mask,  0

function warp_8x8_8bpc_rvv, export=1, ext="v"
    csrw vxrm, zero

    vsetivli zero, 8, e16, m1, ta, ma
    addi sp, sp, -2*15*8
    mv t5, sp
    li t0, 3
    mul t0, a3, t0
    sub a2, a2, t0
    addi a2, a2, -3

    li t0, 64
    addi a3, a3, -8
    li t1, 15
    la t2, dav1d_mc_warp_filter

    lh t6, (a4)
    lh t4, 2(a4)
    vid.v v30
    vwmul.vx v28, v30, t6
1:
    addi t1, t1, -1


    vsetvli zero, zero, e32, m2, ta, ma
    vadd.vx v4, v28, a5
    add a5, a5, t4
    vssra.vi v2, v4, 10
    vadd.vx v2, v2, t0
    vsll.vi v24, v2, 3
    vsetvli zero, zero, e8, mf2, ta, ma

    vluxseg8ei32.v v2, (t2), v24

    vsetvli zero, zero, e16, m1, ta, ma
.irp i, 2, 3, 4, 5, 6, 7, 8, 9
    vle8.v v10, (a2)
    addi a2, a2, 1

    vsext.vf2 v14, v\i
    vzext.vf2 v16, v10

.if \i == 2
    vwmulsu.vv v12, v14, v16
.else
    vwmaccsu.vv v12, v14, v16
.endif
.endr
    vnclip.wi v10, v12, 3

    add a2, a2, a3
    vse16.v v10, (t5)
    addi t5, t5, 16

    bnez t1, 1b

    mv t5, sp
    li t1, 8

    lh t6, 4(a4)
    lh t4, 6(a4)
    vwmul.vx v28, v30, t6
2:
    addi t1, t1, -1

    vsetvli zero, zero, e32, m2, ta, ma
    vadd.vx v4, v28, a6

    add a6, a6, t4
    vssra.vi v2, v4, 10
    vadd.vx v2, v2, t0
    vsll.vi v24, v2, 3
    vsetvli zero, zero, e8, mf2, ta, ma

    vluxseg8ei32.v v2, (t2), v24
    vsetvli zero, zero, e16, m1, ta, ma

.irp i, 2, 3, 4, 5, 6, 7, 8, 9
    vle16.v v10, (t5)
    addi t5, t5, 16

    vsext.vf2 v14, v\i

.if \i == 2
    vwmul.vv v12, v14, v10
.else
    vwmacc.vv v12, v14, v10
.endif
.endr
    addi t5, t5, -16*7
    vnclip.wi v10, v12, 11

    vmax.vx v10, v10, zero
    vsetvli zero, zero, e8, mf2, ta, ma

    vnclipu.wi v12, v10, 0

    vse8.v v12, (a0)
    add a0, a0, a1

    bnez t1, 2b

    addi sp, sp, 2*15*8

    ret
endfunc

function warp_8x8t_8bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero

    vsetivli zero, 8, e16, m1, ta, ma
    addi sp, sp, -2*15*8
    mv t5, sp
    li t0, 3
    mul t0, a3, t0
    sub a2, a2, t0
    addi a2, a2, -3

    li t0, 64
    addi a3, a3, -8
    li t1, 15
    la t2, dav1d_mc_warp_filter

    lh t6, (a4)
    lh t4, 2(a4)
    vid.v v30
    vwmul.vx v28, v30, t6
1:
    addi t1, t1, -1


    vsetvli zero, zero, e32, m2, ta, ma
    vadd.vx v4, v28, a5
    add a5, a5, t4
    vssra.vi v2, v4, 10
    vadd.vx v2, v2, t0
    vsll.vi v24, v2, 3
    vsetvli zero, zero, e8, mf2, ta, ma

    vluxseg8ei32.v v2, (t2), v24

    vsetvli zero, zero, e16, m1, ta, ma
.irp i, 2, 3, 4, 5, 6, 7, 8, 9
    vle8.v v10, (a2)
    addi a2, a2, 1

    vsext.vf2 v14, v\i
    vzext.vf2 v16, v10

.if \i == 2
    vwmulsu.vv v12, v14, v16
.else
    vwmaccsu.vv v12, v14, v16
.endif
.endr
    vnclip.wi v10, v12, 3

    add a2, a2, a3
    vse16.v v10, (t5)
    addi t5, t5, 16

    bnez t1, 1b

    mv t5, sp
    li t1, 8

    lh t6, 4(a4)
    lh t4, 6(a4)
    vwmul.vx v28, v30, t6
2:
    addi t1, t1, -1

    vsetvli zero, zero, e32, m2, ta, ma
    vadd.vx v4, v28, a6
    add a6, a6, t4
    vssra.vi v2, v4, 10
    vadd.vx v2, v2, t0
    vsll.vi v24, v2, 3
    vsetvli zero, zero, e8, mf2, ta, ma

    vluxseg8ei32.v v2, (t2), v24
    vsetvli zero, zero, e16, m1, ta, ma

.irp i, 2, 3, 4, 5, 6, 7, 8, 9
    vle16.v v10, (t5)
    addi t5, t5, 16

    vsext.vf2 v14, v\i

.if \i == 2
    vwmul.vv v12, v14, v10
.else
    vwmacc.vv v12, v14, v10
.endif

.endr
    addi t5, t5, -16*7
    vnclip.wi v10, v12, 7

    vse16.v v10, (a0)
    sh1add a0, a1, a0

    bnez t1, 2b

    addi sp, sp, 2*15*8

    ret
endfunc
