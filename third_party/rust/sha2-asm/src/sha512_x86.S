/* 
 * SHA-512 hash in x86 assembly
 * 
 * Copyright (c) 2014 Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/fast-sha2-hashes-in-x86-assembly
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 */


/* void sha512_compress(uint64_t state[8], const uint8_t block[128]) */
#if defined(__APPLE__) || defined(_WIN32)
.globl _sha512_compress
_sha512_compress:
#else
.globl sha512_compress
sha512_compress:
#endif
    /* 
     * Storage usage:
     *   Bytes  Location    Description
     *       4  eax         Temporary base address of state or block array arguments
     *       4  ecx         Old value of esp
     *       4  esp         x86 stack pointer
     *      64  [esp+ 0]    SHA-512 state variables A,B,C,D,E,F,G,H (8 bytes each)
     *     128  [esp+64]    Circular buffer of most recent 16 key schedule items, 8 bytes each
     *      56  mm0..mm6    Temporary for calculation per round
     *       8  mm7         Control value for byte endian reversal
     *      64  xmm0..xmm3  Temporary for copying or calculation
     */
    
    #define SCHED(i)  (((i)&0xF)*8+64)(%esp)
    #define STATE(i)  (i*8)(%esp)
    
    #define RORQ(reg, shift, temp)  \
        movq   %reg, %temp;         \
        psllq  $(64-shift), %temp;  \
        psrlq  $shift, %reg;        \
        por    %temp, %reg;
    
    #define ROUNDa(i, a, b, c, d, e, f, g, h)  \
        movq    (i*8)(%eax), %mm0;  \
        pshufb  %mm7, %mm0;         \
        movq    %mm0, SCHED(i);     \
        ROUNDTAIL(i, a, b, c, d, e, f, g, h)
    
    #define ROUNDb(i, a, b, c, d, e, f, g, h)  \
        movq   SCHED(i-16), %mm0;  \
        paddq  SCHED(i- 7), %mm0;  \
        movq   SCHED(i-15), %mm1;  \
        movq   %mm1, %mm2;         \
        movq   %mm1, %mm3;         \
        RORQ(mm1, 1, mm5)          \
        RORQ(mm2, 8, mm4)          \
        psrlq  $7, %mm3;           \
        pxor   %mm3, %mm2;         \
        pxor   %mm2, %mm1;         \
        paddq  %mm1, %mm0;         \
        movq   SCHED(i- 2), %mm1;  \
        movq   %mm1, %mm2;         \
        movq   %mm1, %mm3;         \
        RORQ(mm1, 19, mm5)         \
        RORQ(mm2, 61, mm4)         \
        psrlq  $6, %mm3;           \
        pxor   %mm3, %mm2;         \
        pxor   %mm2, %mm1;         \
        paddq  %mm1, %mm0;         \
        movq   %mm0, SCHED(i);     \
        ROUNDTAIL(i, a, b, c, d, e, f, g, h)
    
    #define ROUNDTAIL(i, a, b, c, d, e, f, g, h)  \
        /* Part 0 */                       \
        paddq  STATE(h), %mm0;             \
        movq   STATE(e), %mm1;             \
        movq   %mm1, %mm2;                 \
        movq   %mm1, %mm3;                 \
        RORQ(mm1, 18, mm4)                 \
        RORQ(mm2, 41, mm5)                 \
        RORQ(mm3, 14, mm6)                 \
        pxor   %mm2, %mm1;                 \
        pxor   %mm3, %mm1;                 \
        paddq  .roundconstants+i*8, %mm0;  \
        movq   STATE(g), %mm2;             \
        pxor   STATE(f), %mm2;             \
        pand   STATE(e), %mm2;             \
        pxor   STATE(g), %mm2;             \
        paddq  %mm1, %mm0;                 \
        paddq  %mm2, %mm0;                 \
        /* Part 1 */                       \
        movq   STATE(d), %mm1;             \
        paddq  %mm0, %mm1;                 \
        movq   %mm1, STATE(d);             \
        /* Part 2 */                       \
        movq   STATE(a), %mm1;             \
        movq   %mm1, %mm2;                 \
        movq   %mm1, %mm3;                 \
        RORQ(mm1, 39, mm4)                 \
        RORQ(mm2, 34, mm5)                 \
        RORQ(mm3, 28, mm6)                 \
        pxor   %mm2, %mm1;                 \
        pxor   %mm3, %mm1;                 \
        movq   STATE(c), %mm2;             \
        paddq  %mm1, %mm0;                 \
        movq   %mm2, %mm3;                 \
        por    STATE(b), %mm3;             \
        pand   STATE(b), %mm2;             \
        pand   STATE(a), %mm3;             \
        por    %mm2, %mm3;                 \
        paddq  %mm3, %mm0;                 \
        movq   %mm0, STATE(h);
    
    /* Allocate 16-byte aligned scratch space */
    movl   %esp, %ecx
    subl   $192, %esp
    andl   $~0xF, %esp
    
    /* Copy state */
    movl   4(%ecx), %eax
    movdqu  0(%eax), %xmm0;  movdqu %xmm0,  0(%esp)
    movdqu 16(%eax), %xmm1;  movdqu %xmm1, 16(%esp)
    movdqu 32(%eax), %xmm2;  movdqu %xmm2, 32(%esp)
    movdqu 48(%eax), %xmm3;  movdqu %xmm3, 48(%esp)
    
    /* Do 80 rounds of hashing */
    movl   8(%ecx), %eax
    movq   .bswap64, %mm7
    ROUNDa( 0, 0, 1, 2, 3, 4, 5, 6, 7)
    ROUNDa( 1, 7, 0, 1, 2, 3, 4, 5, 6)
    ROUNDa( 2, 6, 7, 0, 1, 2, 3, 4, 5)
    ROUNDa( 3, 5, 6, 7, 0, 1, 2, 3, 4)
    ROUNDa( 4, 4, 5, 6, 7, 0, 1, 2, 3)
    ROUNDa( 5, 3, 4, 5, 6, 7, 0, 1, 2)
    ROUNDa( 6, 2, 3, 4, 5, 6, 7, 0, 1)
    ROUNDa( 7, 1, 2, 3, 4, 5, 6, 7, 0)
    ROUNDa( 8, 0, 1, 2, 3, 4, 5, 6, 7)
    ROUNDa( 9, 7, 0, 1, 2, 3, 4, 5, 6)
    ROUNDa(10, 6, 7, 0, 1, 2, 3, 4, 5)
    ROUNDa(11, 5, 6, 7, 0, 1, 2, 3, 4)
    ROUNDa(12, 4, 5, 6, 7, 0, 1, 2, 3)
    ROUNDa(13, 3, 4, 5, 6, 7, 0, 1, 2)
    ROUNDa(14, 2, 3, 4, 5, 6, 7, 0, 1)
    ROUNDa(15, 1, 2, 3, 4, 5, 6, 7, 0)
    ROUNDb(16, 0, 1, 2, 3, 4, 5, 6, 7)
    ROUNDb(17, 7, 0, 1, 2, 3, 4, 5, 6)
    ROUNDb(18, 6, 7, 0, 1, 2, 3, 4, 5)
    ROUNDb(19, 5, 6, 7, 0, 1, 2, 3, 4)
    ROUNDb(20, 4, 5, 6, 7, 0, 1, 2, 3)
    ROUNDb(21, 3, 4, 5, 6, 7, 0, 1, 2)
    ROUNDb(22, 2, 3, 4, 5, 6, 7, 0, 1)
    ROUNDb(23, 1, 2, 3, 4, 5, 6, 7, 0)
    ROUNDb(24, 0, 1, 2, 3, 4, 5, 6, 7)
    ROUNDb(25, 7, 0, 1, 2, 3, 4, 5, 6)
    ROUNDb(26, 6, 7, 0, 1, 2, 3, 4, 5)
    ROUNDb(27, 5, 6, 7, 0, 1, 2, 3, 4)
    ROUNDb(28, 4, 5, 6, 7, 0, 1, 2, 3)
    ROUNDb(29, 3, 4, 5, 6, 7, 0, 1, 2)
    ROUNDb(30, 2, 3, 4, 5, 6, 7, 0, 1)
    ROUNDb(31, 1, 2, 3, 4, 5, 6, 7, 0)
    ROUNDb(32, 0, 1, 2, 3, 4, 5, 6, 7)
    ROUNDb(33, 7, 0, 1, 2, 3, 4, 5, 6)
    ROUNDb(34, 6, 7, 0, 1, 2, 3, 4, 5)
    ROUNDb(35, 5, 6, 7, 0, 1, 2, 3, 4)
    ROUNDb(36, 4, 5, 6, 7, 0, 1, 2, 3)
    ROUNDb(37, 3, 4, 5, 6, 7, 0, 1, 2)
    ROUNDb(38, 2, 3, 4, 5, 6, 7, 0, 1)
    ROUNDb(39, 1, 2, 3, 4, 5, 6, 7, 0)
    ROUNDb(40, 0, 1, 2, 3, 4, 5, 6, 7)
    ROUNDb(41, 7, 0, 1, 2, 3, 4, 5, 6)
    ROUNDb(42, 6, 7, 0, 1, 2, 3, 4, 5)
    ROUNDb(43, 5, 6, 7, 0, 1, 2, 3, 4)
    ROUNDb(44, 4, 5, 6, 7, 0, 1, 2, 3)
    ROUNDb(45, 3, 4, 5, 6, 7, 0, 1, 2)
    ROUNDb(46, 2, 3, 4, 5, 6, 7, 0, 1)
    ROUNDb(47, 1, 2, 3, 4, 5, 6, 7, 0)
    ROUNDb(48, 0, 1, 2, 3, 4, 5, 6, 7)
    ROUNDb(49, 7, 0, 1, 2, 3, 4, 5, 6)
    ROUNDb(50, 6, 7, 0, 1, 2, 3, 4, 5)
    ROUNDb(51, 5, 6, 7, 0, 1, 2, 3, 4)
    ROUNDb(52, 4, 5, 6, 7, 0, 1, 2, 3)
    ROUNDb(53, 3, 4, 5, 6, 7, 0, 1, 2)
    ROUNDb(54, 2, 3, 4, 5, 6, 7, 0, 1)
    ROUNDb(55, 1, 2, 3, 4, 5, 6, 7, 0)
    ROUNDb(56, 0, 1, 2, 3, 4, 5, 6, 7)
    ROUNDb(57, 7, 0, 1, 2, 3, 4, 5, 6)
    ROUNDb(58, 6, 7, 0, 1, 2, 3, 4, 5)
    ROUNDb(59, 5, 6, 7, 0, 1, 2, 3, 4)
    ROUNDb(60, 4, 5, 6, 7, 0, 1, 2, 3)
    ROUNDb(61, 3, 4, 5, 6, 7, 0, 1, 2)
    ROUNDb(62, 2, 3, 4, 5, 6, 7, 0, 1)
    ROUNDb(63, 1, 2, 3, 4, 5, 6, 7, 0)
    ROUNDb(64, 0, 1, 2, 3, 4, 5, 6, 7)
    ROUNDb(65, 7, 0, 1, 2, 3, 4, 5, 6)
    ROUNDb(66, 6, 7, 0, 1, 2, 3, 4, 5)
    ROUNDb(67, 5, 6, 7, 0, 1, 2, 3, 4)
    ROUNDb(68, 4, 5, 6, 7, 0, 1, 2, 3)
    ROUNDb(69, 3, 4, 5, 6, 7, 0, 1, 2)
    ROUNDb(70, 2, 3, 4, 5, 6, 7, 0, 1)
    ROUNDb(71, 1, 2, 3, 4, 5, 6, 7, 0)
    ROUNDb(72, 0, 1, 2, 3, 4, 5, 6, 7)
    ROUNDb(73, 7, 0, 1, 2, 3, 4, 5, 6)
    ROUNDb(74, 6, 7, 0, 1, 2, 3, 4, 5)
    ROUNDb(75, 5, 6, 7, 0, 1, 2, 3, 4)
    ROUNDb(76, 4, 5, 6, 7, 0, 1, 2, 3)
    ROUNDb(77, 3, 4, 5, 6, 7, 0, 1, 2)
    ROUNDb(78, 2, 3, 4, 5, 6, 7, 0, 1)
    ROUNDb(79, 1, 2, 3, 4, 5, 6, 7, 0)
    
    /* Add to state */
    movl   4(%ecx), %eax
    movdqu  0(%eax), %xmm0;  paddq  0(%esp), %xmm0;  movdqu %xmm0,  0(%eax)
    movdqu 16(%eax), %xmm1;  paddq 16(%esp), %xmm1;  movdqu %xmm1, 16(%eax)
    movdqu 32(%eax), %xmm2;  paddq 32(%esp), %xmm2;  movdqu %xmm2, 32(%eax)
    movdqu 48(%eax), %xmm3;  paddq 48(%esp), %xmm3;  movdqu %xmm3, 48(%eax)
    
    /* Clean up */
    emms
    movl   %ecx, %esp
    retl


.balign 8
.bswap64:
    .quad 0x0001020304050607

.roundconstants:
    .quad 0x428A2F98D728AE22, 0x7137449123EF65CD, 0xB5C0FBCFEC4D3B2F, 0xE9B5DBA58189DBBC
    .quad 0x3956C25BF348B538, 0x59F111F1B605D019, 0x923F82A4AF194F9B, 0xAB1C5ED5DA6D8118
    .quad 0xD807AA98A3030242, 0x12835B0145706FBE, 0x243185BE4EE4B28C, 0x550C7DC3D5FFB4E2
    .quad 0x72BE5D74F27B896F, 0x80DEB1FE3B1696B1, 0x9BDC06A725C71235, 0xC19BF174CF692694
    .quad 0xE49B69C19EF14AD2, 0xEFBE4786384F25E3, 0x0FC19DC68B8CD5B5, 0x240CA1CC77AC9C65
    .quad 0x2DE92C6F592B0275, 0x4A7484AA6EA6E483, 0x5CB0A9DCBD41FBD4, 0x76F988DA831153B5
    .quad 0x983E5152EE66DFAB, 0xA831C66D2DB43210, 0xB00327C898FB213F, 0xBF597FC7BEEF0EE4
    .quad 0xC6E00BF33DA88FC2, 0xD5A79147930AA725, 0x06CA6351E003826F, 0x142929670A0E6E70
    .quad 0x27B70A8546D22FFC, 0x2E1B21385C26C926, 0x4D2C6DFC5AC42AED, 0x53380D139D95B3DF
    .quad 0x650A73548BAF63DE, 0x766A0ABB3C77B2A8, 0x81C2C92E47EDAEE6, 0x92722C851482353B
    .quad 0xA2BFE8A14CF10364, 0xA81A664BBC423001, 0xC24B8B70D0F89791, 0xC76C51A30654BE30
    .quad 0xD192E819D6EF5218, 0xD69906245565A910, 0xF40E35855771202A, 0x106AA07032BBD1B8
    .quad 0x19A4C116B8D2D0C8, 0x1E376C085141AB53, 0x2748774CDF8EEB99, 0x34B0BCB5E19B48A8
    .quad 0x391C0CB3C5C95A63, 0x4ED8AA4AE3418ACB, 0x5B9CCA4F7763E373, 0x682E6FF3D6B2B8A3
    .quad 0x748F82EE5DEFB2FC, 0x78A5636F43172F60, 0x84C87814A1F0AB72, 0x8CC702081A6439EC
    .quad 0x90BEFFFA23631E28, 0xA4506CEBDE82BDE9, 0xBEF9A3F7B2C67915, 0xC67178F2E372532B
    .quad 0xCA273ECEEA26619C, 0xD186B8C721C0C207, 0xEADA7DD6CDE0EB1E, 0xF57D4F7FEE6ED178
    .quad 0x06F067AA72176FBA, 0x0A637DC5A2C898A6, 0x113F9804BEF90DAE, 0x1B710B35131C471B
    .quad 0x28DB77F523047D84, 0x32CAAB7B40C72493, 0x3C9EBE0A15C9BEBC, 0x431D67C49C100D4C
    .quad 0x4CC5D4BECB3E42B6, 0x597F299CFC657E2A, 0x5FCB6FAB3AD6FAEC, 0x6C44198C4A475817
