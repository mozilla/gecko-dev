/*
 * SHA-256 hash in AArch64 assembly for macos/M1
 *
 * Based on the following C intrinsics implementation:
 * <https://github.com/noloader/SHA-Intrinsics/blob/master/sha256-arm.c>
 *
 * Original C written and placed in public domain by Jeffrey Walton.
 * Based on code from ARM, and by Johannes Schneiders, Skip Hovsmith and
 * Barry O'Rourke for the mbedTLS project.
 */


/* void sha256_compress(uint32_t state[8], const uint8_t block[64]) */
.global _sha256_compress
_sha256_compress:
        mov     x8, #0
        ldp     q0, q1, [x0]
        ldp     q2, q3, [x1]
        ldp     q4, q5, [x1, #32]
        stp     q2, q3, [sp, #-64]!
        stp     q4, q5, [sp, #32]
        mov     x9, sp
LBB0_1:
        ldr     q2, [x9, x8]
        rev32.16b       v2, v2
        str     q2, [x9, x8]
        add     x8, x8, #16
        cmp     x8, #64
        b.ne    LBB0_1
        adrp    x8, K_0@PAGE
        ldr     q2, [x8, K_0@PAGEOFF]
        ldp     q6, q4, [sp]
        add.4s  v3, v6, v2

        // Rounds 0-3
        sha256su0.4s    v6, v4
        adrp    x8, K_1@PAGE
        ldr     q2, [x8, K_1@PAGEOFF]
        add.4s  v7, v4, v2
        mov.16b v16, v0
        sha256h.4s      q16, q1, v3
        mov.16b v2, v1
        sha256h2.4s     q2, q0, v3
        ldp     q5, q3, [sp, #32]
        sha256su1.4s    v6, v5, v3

        // Rounds 4-7
        sha256su0.4s    v4, v5
        adrp    x8, K_2@PAGE
        ldr     q17, [x8, K_2@PAGEOFF]
        add.4s  v17, v5, v17
        mov.16b v18, v16
        sha256h.4s      q18, q2, v7
        sha256h2.4s     q2, q16, v7
        sha256su1.4s    v4, v3, v6

        // Rounds 8-11
        sha256su0.4s    v5, v3
        adrp    x8, K_3@PAGE
        ldr     q7, [x8, K_3@PAGEOFF]
        add.4s  v7, v3, v7
        mov.16b v16, v18
        sha256h.4s      q16, q2, v17
        sha256h2.4s     q2, q18, v17
        sha256su1.4s    v5, v6, v4

        // Rounds 12-15
        sha256su0.4s    v3, v6
        adrp    x8, K_4@PAGE
        ldr     q17, [x8, K_4@PAGEOFF]
        add.4s  v17, v6, v17
        mov.16b v18, v16
        sha256h.4s      q18, q2, v7
        sha256h2.4s     q2, q16, v7
        sha256su1.4s    v3, v4, v5

        // Rounds 16-19
        sha256su0.4s    v6, v4
        adrp    x8, K_5@PAGE
        ldr     q7, [x8, K_5@PAGEOFF]
        add.4s  v7, v4, v7
        mov.16b v16, v18
        sha256h.4s      q16, q2, v17
        sha256h2.4s     q2, q18, v17
        sha256su1.4s    v6, v5, v3

        // Rounds 20-23
        sha256su0.4s    v4, v5
        adrp    x8, K_6@PAGE
        ldr     q17, [x8, K_6@PAGEOFF]
        add.4s  v17, v5, v17
        mov.16b v18, v16
        sha256h.4s      q18, q2, v7
        sha256h2.4s     q2, q16, v7
        sha256su1.4s    v4, v3, v6

        // Rounds 24-27
        sha256su0.4s    v5, v3
        adrp    x8, K_7@PAGE
        ldr     q7, [x8, K_7@PAGEOFF]
        add.4s  v7, v3, v7
        mov.16b v16, v18
        sha256h.4s      q16, q2, v17
        sha256h2.4s     q2, q18, v17
        sha256su1.4s    v5, v6, v4

        // Rounds 28-31
        sha256su0.4s    v3, v6
        adrp    x8, K_8@PAGE
        ldr     q17, [x8, K_8@PAGEOFF]
        add.4s  v17, v6, v17
        mov.16b v18, v16
        sha256h.4s      q18, q2, v7
        sha256h2.4s     q2, q16, v7
        sha256su1.4s    v3, v4, v5

        // Rounds 32-35
        sha256su0.4s    v6, v4
        adrp    x8, K_9@PAGE
        ldr     q7, [x8, K_9@PAGEOFF]
        add.4s  v7, v4, v7
        mov.16b v16, v18
        sha256h.4s      q16, q2, v17
        sha256h2.4s     q2, q18, v17
        sha256su1.4s    v6, v5, v3

        // Rounds 36-39
        sha256su0.4s    v4, v5
        adrp    x8, K_10@PAGE
        ldr     q17, [x8, K_10@PAGEOFF]
        add.4s  v17, v5, v17
        mov.16b v18, v16
        sha256h.4s      q18, q2, v7
        sha256h2.4s     q2, q16, v7
        sha256su1.4s    v4, v3, v6

        // Rounds 40-43
        sha256su0.4s    v5, v3
        adrp    x8, K_11@PAGE
        ldr     q7, [x8, K_11@PAGEOFF]
        add.4s  v7, v3, v7
        mov.16b v16, v18
        sha256h.4s      q16, q2, v17
        sha256h2.4s     q2, q18, v17
        sha256su1.4s    v5, v6, v4

        // Rounds 44-47
        sha256su0.4s    v3, v6
        adrp    x8, K_12@PAGE
        ldr     q17, [x8, K_12@PAGEOFF]
        add.4s  v6, v6, v17
        mov.16b v17, v16
        sha256h.4s      q17, q2, v7
        sha256h2.4s     q2, q16, v7
        sha256su1.4s    v3, v4, v5

        // Rounds 48-51
        adrp    x8, K_13@PAGE
        ldr     q7, [x8, K_13@PAGEOFF]
        add.4s  v4, v4, v7
        mov.16b v7, v17
        sha256h.4s      q7, q2, v6
        sha256h2.4s     q2, q17, v6

        // Rounds 52-55
        adrp    x8, K_14@PAGE
        ldr     q6, [x8, K_14@PAGEOFF]
        add.4s  v5, v5, v6
        mov.16b v6, v7
        sha256h.4s      q6, q2, v4
        sha256h2.4s     q2, q7, v4

        // Rounds 56-59
        adrp    x8, K_15@PAGE
        ldr     q4, [x8, K_15@PAGEOFF]
        add.4s  v3, v3, v4
        mov.16b v4, v6
        sha256h.4s      q4, q2, v5
        sha256h2.4s     q2, q6, v5
        
        // Rounds 60-63
        mov.16b v5, v4
        sha256h.4s      q5, q2, v3
        sha256h2.4s     q2, q4, v3

        // Update state
        add.4s  v0, v5, v0
        add.4s  v1, v2, v1

        // restore
        stp     q0, q1, [x0]
        add     sp, sp, #64

        ret


.align 4
K_0:
        .long   1116352408
        .long   1899447441
        .long   3049323471
        .long   3921009573
.align 4
K_1:
        .long   961987163
        .long   1508970993
        .long   2453635748
        .long   2870763221
.align 4
K_2:
        .long   3624381080
        .long   310598401
        .long   607225278
        .long   1426881987
.align 4
K_3:
        .long   1925078388
        .long   2162078206
        .long   2614888103
        .long   3248222580
.align 4
K_4:
        .long   3835390401
        .long   4022224774
        .long   264347078
        .long   604807628
.align 4
K_5:
        .long   770255983
        .long   1249150122
        .long   1555081692
        .long   1996064986
.align 4
K_6:
        .long   2554220882
        .long   2821834349
        .long   2952996808
        .long   3210313671
.align 4
K_7:
        .long   3336571891
        .long   3584528711
        .long   113926993
        .long   338241895
.align 4
K_8:
        .long   666307205
        .long   773529912
        .long   1294757372
        .long   1396182291
.align 4
K_9:
        .long   1695183700
        .long   1986661051
        .long   2177026350
        .long   2456956037
.align 4
K_10:
        .long   2730485921
        .long   2820302411
        .long   3259730800
        .long   3345764771
.align 4
K_11:
        .long   3516065817
        .long   3600352804
        .long   4094571909
        .long   275423344
.align 4
K_12:
        .long   430227734
        .long   506948616
        .long   659060556
        .long   883997877
.align 4
K_13:
        .long   958139571
        .long   1322822218
        .long   1537002063
        .long   1747873779
.align 4
K_14:
        .long   1955562222
        .long   2024104815
        .long   2227730452
        .long   2361852424
.align 4
K_15:
        .long   2428436474
        .long   2756734187
        .long   3204031479
        .long   3329325298
