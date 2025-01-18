/* 
 * SHA-512 hash in x86-64 assembly
 * 
 * Copyright (c) 2017 Project Nayuki. (MIT License)
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
#ifdef __APPLE__
.globl _sha512_compress
_sha512_compress:
#else
.globl sha512_compress
sha512_compress:
#endif
    /* 
     * Storage usage:
     *   Bytes  Location  Description
     *       8  rax       Temporary for calculation per round
     *       8  rbx       Temporary for calculation per round
     *       8  rcx       Temporary for calculation per round
     *       8  rdx       Temporary for calculation per round
     *       8  rsi       Base address of block array argument (read-only)
     *       8  rdi       Base address of state array argument (read-only)
     *       8  rsp       x86-64 stack pointer
     *       8  r8        SHA-512 state variable A
     *       8  r9        SHA-512 state variable B
     *       8  r10       SHA-512 state variable C
     *       8  r11       SHA-512 state variable D
     *       8  r12       SHA-512 state variable E
     *       8  r13       SHA-512 state variable F
     *       8  r14       SHA-512 state variable G
     *       8  r15       SHA-512 state variable H
     *     128  [rsp+0]   Circular buffer of most recent 16 key schedule items, 8 bytes each
     *      16  xmm0      Caller's value of r10 (only low 64 bits are used)
     *      16  xmm1      Caller's value of r11 (only low 64 bits are used)
     *      16  xmm2      Caller's value of r12 (only low 64 bits are used)
     *      16  xmm3      Caller's value of r13 (only low 64 bits are used)
     *      16  xmm4      Caller's value of r14 (only low 64 bits are used)
     *      16  xmm5      Caller's value of r15 (only low 64 bits are used)
     *      16  xmm6      Caller's value of rbx (only low 64 bits are used)
     */
    
    #define SCHED(i)  (((i)&0xF)*8)(%rsp)
    
    #define ROUNDa(i, a, b, c, d, e, f, g, h, k)  \
        movq    (i*8)(%rsi), %rbx;  \
        bswapq  %rbx;               \
        movq    %rbx, SCHED(i);     \
        ROUNDTAIL(a, b, c, d, e, f, g, h, k)
    
    #define ROUNDb(i, a, b, c, d, e, f, g, h, k)  \
        movq  SCHED(i-15), %rax;  \
        movq  SCHED(i-16), %rbx;  \
        addq  SCHED(i- 7), %rbx;  \
        movq  %rax, %rcx;         \
        movq  %rax, %rdx;         \
        rorq  $8, %rcx;           \
        shrq  $7, %rdx;           \
        rorq  $1, %rax;           \
        xorq  %rdx, %rcx;         \
        xorq  %rcx, %rax;         \
        addq  %rax, %rbx;         \
        movq  SCHED(i- 2), %rax;  \
        movq  %rax, %rcx;         \
        movq  %rax, %rdx;         \
        rorq  $61, %rcx;          \
        shrq  $6, %rdx;           \
        rorq  $19, %rax;          \
        xorq  %rdx, %rcx;         \
        xorq  %rcx, %rax;         \
        addq  %rax, %rbx;         \
        movq  %rbx, SCHED(i);     \
        ROUNDTAIL(a, b, c, d, e, f, g, h, k)
    
    #define ROUNDTAIL(a, b, c, d, e, f, g, h, k)  \
        /* Part 0 */       \
        /* ROR transformation inspired by Intel's SHA-256 implementation */ \
        movq  %e, %rax;    \
        rorq  $23, %rax;    \
        xorq  %e, %rax;    \
        rorq  $4, %rax;    \
        xorq  %e, %rax;    \
        rorq  $14, %rax;   \
        addq  %rbx, %h;    \
        movq  %g, %rcx;    \
        xorq  %f, %rcx;    \
        andq  %e, %rcx;    \
        xorq  %g, %rcx;    \
        addq  %rax, %h;    \
        movabs $k, %rax;   \
        addq  %rcx, %h;    \
        addq  %rax, %h;    \
        /* Part 1 */       \
        addq  %h, %d;      \
        /* Part 2 */       \
        /* ROR transformation inspired by Intel's SHA-256 implementation */ \
        movq  %a, %rax;    \
        rorq  $5, %rax;    \
        xorq  %a, %rax;    \
        rorq  $6, %rax;    \
        xorq  %a, %rax;    \
        rorq  $28, %rax;   \
        movq  %c, %rcx;    \
        addq  %rax, %h;    \
        movq  %c, %rax;    \
        orq   %b, %rax;    \
        andq  %b, %rcx;    \
        andq  %a, %rax;    \
        orq   %rcx, %rax;  \
        addq  %rax, %h;
    
    /* Save registers, allocate scratch space */
    movq  %r10, %xmm0
    movq  %r11, %xmm1
    movq  %r12, %xmm2
    movq  %r13, %xmm3
    movq  %r14, %xmm4
    movq  %r15, %xmm5
    movq  %rbx, %xmm6
    subq  $128, %rsp
    
    /* Load state */
    movq   0(%rdi), %r8   /* a */
    movq   8(%rdi), %r9   /* b */
    movq  16(%rdi), %r10  /* c */
    movq  24(%rdi), %r11  /* d */
    movq  32(%rdi), %r12  /* e */
    movq  40(%rdi), %r13  /* f */
    movq  48(%rdi), %r14  /* g */
    movq  56(%rdi), %r15  /* h */
    
    /* Do 80 rounds of hashing */
    ROUNDa( 0, r8 , r9 , r10, r11, r12, r13, r14, r15, 0x428A2F98D728AE22)
    ROUNDa( 1, r15, r8 , r9 , r10, r11, r12, r13, r14, 0x7137449123EF65CD)
    ROUNDa( 2, r14, r15, r8 , r9 , r10, r11, r12, r13, 0xB5C0FBCFEC4D3B2F)
    ROUNDa( 3, r13, r14, r15, r8 , r9 , r10, r11, r12, 0xE9B5DBA58189DBBC)
    ROUNDa( 4, r12, r13, r14, r15, r8 , r9 , r10, r11, 0x3956C25BF348B538)
    ROUNDa( 5, r11, r12, r13, r14, r15, r8 , r9 , r10, 0x59F111F1B605D019)
    ROUNDa( 6, r10, r11, r12, r13, r14, r15, r8 , r9 , 0x923F82A4AF194F9B)
    ROUNDa( 7, r9 , r10, r11, r12, r13, r14, r15, r8 , 0xAB1C5ED5DA6D8118)
    ROUNDa( 8, r8 , r9 , r10, r11, r12, r13, r14, r15, 0xD807AA98A3030242)
    ROUNDa( 9, r15, r8 , r9 , r10, r11, r12, r13, r14, 0x12835B0145706FBE)
    ROUNDa(10, r14, r15, r8 , r9 , r10, r11, r12, r13, 0x243185BE4EE4B28C)
    ROUNDa(11, r13, r14, r15, r8 , r9 , r10, r11, r12, 0x550C7DC3D5FFB4E2)
    ROUNDa(12, r12, r13, r14, r15, r8 , r9 , r10, r11, 0x72BE5D74F27B896F)
    ROUNDa(13, r11, r12, r13, r14, r15, r8 , r9 , r10, 0x80DEB1FE3B1696B1)
    ROUNDa(14, r10, r11, r12, r13, r14, r15, r8 , r9 , 0x9BDC06A725C71235)
    ROUNDa(15, r9 , r10, r11, r12, r13, r14, r15, r8 , 0xC19BF174CF692694)
    ROUNDb(16, r8 , r9 , r10, r11, r12, r13, r14, r15, 0xE49B69C19EF14AD2)
    ROUNDb(17, r15, r8 , r9 , r10, r11, r12, r13, r14, 0xEFBE4786384F25E3)
    ROUNDb(18, r14, r15, r8 , r9 , r10, r11, r12, r13, 0x0FC19DC68B8CD5B5)
    ROUNDb(19, r13, r14, r15, r8 , r9 , r10, r11, r12, 0x240CA1CC77AC9C65)
    ROUNDb(20, r12, r13, r14, r15, r8 , r9 , r10, r11, 0x2DE92C6F592B0275)
    ROUNDb(21, r11, r12, r13, r14, r15, r8 , r9 , r10, 0x4A7484AA6EA6E483)
    ROUNDb(22, r10, r11, r12, r13, r14, r15, r8 , r9 , 0x5CB0A9DCBD41FBD4)
    ROUNDb(23, r9 , r10, r11, r12, r13, r14, r15, r8 , 0x76F988DA831153B5)
    ROUNDb(24, r8 , r9 , r10, r11, r12, r13, r14, r15, 0x983E5152EE66DFAB)
    ROUNDb(25, r15, r8 , r9 , r10, r11, r12, r13, r14, 0xA831C66D2DB43210)
    ROUNDb(26, r14, r15, r8 , r9 , r10, r11, r12, r13, 0xB00327C898FB213F)
    ROUNDb(27, r13, r14, r15, r8 , r9 , r10, r11, r12, 0xBF597FC7BEEF0EE4)
    ROUNDb(28, r12, r13, r14, r15, r8 , r9 , r10, r11, 0xC6E00BF33DA88FC2)
    ROUNDb(29, r11, r12, r13, r14, r15, r8 , r9 , r10, 0xD5A79147930AA725)
    ROUNDb(30, r10, r11, r12, r13, r14, r15, r8 , r9 , 0x06CA6351E003826F)
    ROUNDb(31, r9 , r10, r11, r12, r13, r14, r15, r8 , 0x142929670A0E6E70)
    ROUNDb(32, r8 , r9 , r10, r11, r12, r13, r14, r15, 0x27B70A8546D22FFC)
    ROUNDb(33, r15, r8 , r9 , r10, r11, r12, r13, r14, 0x2E1B21385C26C926)
    ROUNDb(34, r14, r15, r8 , r9 , r10, r11, r12, r13, 0x4D2C6DFC5AC42AED)
    ROUNDb(35, r13, r14, r15, r8 , r9 , r10, r11, r12, 0x53380D139D95B3DF)
    ROUNDb(36, r12, r13, r14, r15, r8 , r9 , r10, r11, 0x650A73548BAF63DE)
    ROUNDb(37, r11, r12, r13, r14, r15, r8 , r9 , r10, 0x766A0ABB3C77B2A8)
    ROUNDb(38, r10, r11, r12, r13, r14, r15, r8 , r9 , 0x81C2C92E47EDAEE6)
    ROUNDb(39, r9 , r10, r11, r12, r13, r14, r15, r8 , 0x92722C851482353B)
    ROUNDb(40, r8 , r9 , r10, r11, r12, r13, r14, r15, 0xA2BFE8A14CF10364)
    ROUNDb(41, r15, r8 , r9 , r10, r11, r12, r13, r14, 0xA81A664BBC423001)
    ROUNDb(42, r14, r15, r8 , r9 , r10, r11, r12, r13, 0xC24B8B70D0F89791)
    ROUNDb(43, r13, r14, r15, r8 , r9 , r10, r11, r12, 0xC76C51A30654BE30)
    ROUNDb(44, r12, r13, r14, r15, r8 , r9 , r10, r11, 0xD192E819D6EF5218)
    ROUNDb(45, r11, r12, r13, r14, r15, r8 , r9 , r10, 0xD69906245565A910)
    ROUNDb(46, r10, r11, r12, r13, r14, r15, r8 , r9 , 0xF40E35855771202A)
    ROUNDb(47, r9 , r10, r11, r12, r13, r14, r15, r8 , 0x106AA07032BBD1B8)
    ROUNDb(48, r8 , r9 , r10, r11, r12, r13, r14, r15, 0x19A4C116B8D2D0C8)
    ROUNDb(49, r15, r8 , r9 , r10, r11, r12, r13, r14, 0x1E376C085141AB53)
    ROUNDb(50, r14, r15, r8 , r9 , r10, r11, r12, r13, 0x2748774CDF8EEB99)
    ROUNDb(51, r13, r14, r15, r8 , r9 , r10, r11, r12, 0x34B0BCB5E19B48A8)
    ROUNDb(52, r12, r13, r14, r15, r8 , r9 , r10, r11, 0x391C0CB3C5C95A63)
    ROUNDb(53, r11, r12, r13, r14, r15, r8 , r9 , r10, 0x4ED8AA4AE3418ACB)
    ROUNDb(54, r10, r11, r12, r13, r14, r15, r8 , r9 , 0x5B9CCA4F7763E373)
    ROUNDb(55, r9 , r10, r11, r12, r13, r14, r15, r8 , 0x682E6FF3D6B2B8A3)
    ROUNDb(56, r8 , r9 , r10, r11, r12, r13, r14, r15, 0x748F82EE5DEFB2FC)
    ROUNDb(57, r15, r8 , r9 , r10, r11, r12, r13, r14, 0x78A5636F43172F60)
    ROUNDb(58, r14, r15, r8 , r9 , r10, r11, r12, r13, 0x84C87814A1F0AB72)
    ROUNDb(59, r13, r14, r15, r8 , r9 , r10, r11, r12, 0x8CC702081A6439EC)
    ROUNDb(60, r12, r13, r14, r15, r8 , r9 , r10, r11, 0x90BEFFFA23631E28)
    ROUNDb(61, r11, r12, r13, r14, r15, r8 , r9 , r10, 0xA4506CEBDE82BDE9)
    ROUNDb(62, r10, r11, r12, r13, r14, r15, r8 , r9 , 0xBEF9A3F7B2C67915)
    ROUNDb(63, r9 , r10, r11, r12, r13, r14, r15, r8 , 0xC67178F2E372532B)
    ROUNDb(64, r8 , r9 , r10, r11, r12, r13, r14, r15, 0xCA273ECEEA26619C)
    ROUNDb(65, r15, r8 , r9 , r10, r11, r12, r13, r14, 0xD186B8C721C0C207)
    ROUNDb(66, r14, r15, r8 , r9 , r10, r11, r12, r13, 0xEADA7DD6CDE0EB1E)
    ROUNDb(67, r13, r14, r15, r8 , r9 , r10, r11, r12, 0xF57D4F7FEE6ED178)
    ROUNDb(68, r12, r13, r14, r15, r8 , r9 , r10, r11, 0x06F067AA72176FBA)
    ROUNDb(69, r11, r12, r13, r14, r15, r8 , r9 , r10, 0x0A637DC5A2C898A6)
    ROUNDb(70, r10, r11, r12, r13, r14, r15, r8 , r9 , 0x113F9804BEF90DAE)
    ROUNDb(71, r9 , r10, r11, r12, r13, r14, r15, r8 , 0x1B710B35131C471B)
    ROUNDb(72, r8 , r9 , r10, r11, r12, r13, r14, r15, 0x28DB77F523047D84)
    ROUNDb(73, r15, r8 , r9 , r10, r11, r12, r13, r14, 0x32CAAB7B40C72493)
    ROUNDb(74, r14, r15, r8 , r9 , r10, r11, r12, r13, 0x3C9EBE0A15C9BEBC)
    ROUNDb(75, r13, r14, r15, r8 , r9 , r10, r11, r12, 0x431D67C49C100D4C)
    ROUNDb(76, r12, r13, r14, r15, r8 , r9 , r10, r11, 0x4CC5D4BECB3E42B6)
    ROUNDb(77, r11, r12, r13, r14, r15, r8 , r9 , r10, 0x597F299CFC657E2A)
    ROUNDb(78, r10, r11, r12, r13, r14, r15, r8 , r9 , 0x5FCB6FAB3AD6FAEC)
    ROUNDb(79, r9 , r10, r11, r12, r13, r14, r15, r8 , 0x6C44198C4A475817)
    
    /* Add to state */
    addq  %r8 ,  0(%rdi)
    addq  %r9 ,  8(%rdi)
    addq  %r10, 16(%rdi)
    addq  %r11, 24(%rdi)
    addq  %r12, 32(%rdi)
    addq  %r13, 40(%rdi)
    addq  %r14, 48(%rdi)
    addq  %r15, 56(%rdi)
    
    /* Restore registers */
    movq  %xmm0, %r10
    movq  %xmm1, %r11
    movq  %xmm2, %r12
    movq  %xmm3, %r13
    movq  %xmm4, %r14
    movq  %xmm5, %r15
    movq  %xmm6, %rbx
    addq  $128, %rsp
    retq
