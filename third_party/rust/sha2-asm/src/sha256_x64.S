/* 
 * SHA-256 hash in x86-64 assembly
 * 
 * Copyright (c) 2015 Project Nayuki. (MIT License)
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


/* void sha256_compress(uint32_t state[8], const uint8_t block[64]) */
#ifdef __APPLE__
.globl _sha256_compress
_sha256_compress:
#else
.globl sha256_compress
sha256_compress:
#endif
    /* 
     * Storage usage:
     *   Bytes  Location  Description
     *       4  eax       Temporary for calculation per round
     *       4  ebx       Temporary for calculation per round
     *       4  ecx       Temporary for calculation per round
     *       4  edx       Temporary for calculation per round
     *       8  rsi       Base address of block array argument (read-only)
     *       8  rdi       Base address of state array argument (read-only)
     *       8  rsp       x86-64 stack pointer
     *       4  r8d       SHA-256 state variable A
     *       4  r9d       SHA-256 state variable B
     *       4  r10d      SHA-256 state variable C
     *       4  r11d      SHA-256 state variable D
     *       4  r12d      SHA-256 state variable E
     *       4  r13d      SHA-256 state variable F
     *       4  r14d      SHA-256 state variable G
     *       4  r15d      SHA-256 state variable H
     *      64  [rsp+0]   Circular buffer of most recent 16 key schedule items, 4 bytes each
     *      16  xmm0      Caller's value of r10 (only low 64 bits are used)
     *      16  xmm1      Caller's value of r11 (only low 64 bits are used)
     *      16  xmm2      Caller's value of r12 (only low 64 bits are used)
     *      16  xmm3      Caller's value of r13 (only low 64 bits are used)
     *      16  xmm4      Caller's value of r14 (only low 64 bits are used)
     *      16  xmm5      Caller's value of r15 (only low 64 bits are used)
     *      16  xmm6      Caller's value of rbx (only low 64 bits are used)
     */
    
    #define SCHED(i)  (((i)&0xF)*4)(%rsp)
    
    #define ROUNDa(i, a, b, c, d, e, f, g, h, k)  \
        movl    (i*4)(%rsi), %ebx;  \
        bswapl  %ebx;               \
        movl    %ebx, SCHED(i);     \
        ROUNDTAIL(a, b, c, d, e, f, g, h, k)
    
    #define ROUNDb(i, a, b, c, d, e, f, g, h, k)  \
        movl  SCHED(i-15), %eax;  \
        movl  SCHED(i-16), %ebx;  \
        addl  SCHED(i- 7), %ebx;  \
        movl  %eax, %ecx;         \
        movl  %eax, %edx;         \
        rorl  $18, %ecx;          \
        shrl  $3, %edx;           \
        rorl  $7, %eax;           \
        xorl  %edx, %ecx;         \
        xorl  %ecx, %eax;         \
        addl  %eax, %ebx;         \
        movl  SCHED(i- 2), %eax;  \
        movl  %eax, %ecx;         \
        movl  %eax, %edx;         \
        rorl  $19, %ecx;          \
        shrl  $10, %edx;          \
        rorl  $17, %eax;          \
        xorl  %edx, %ecx;         \
        xorl  %ecx, %eax;         \
        addl  %eax, %ebx;         \
        movl  %ebx, SCHED(i);     \
        ROUNDTAIL(a, b, c, d, e, f, g, h, k)
    
    #define ROUNDTAIL(a, b, c, d, e, f, g, h, k)  \
        /* Part 0 */               \
        /* See Intel's "Fast SHA-256 Implementations" for the ROR transformation */ \
        movl  %e, %eax;            \
        rorl  $14, %eax;            \
        xorl  %e, %eax;            \
        rorl  $5, %eax;            \
        xorl  %e, %eax;            \
        rorl  $6, %eax;            \
        addl  %ebx, %h;            \
        movl  %g, %ecx;            \
        xorl  %f, %ecx;            \
        andl  %e, %ecx;            \
        xorl  %g, %ecx;            \
        leal  k(%rax,%rcx), %eax;  \
        addl  %eax, %h;            \
        /* Part 1 */               \
        addl  %h, %d;              \
        /* Part 2 */               \
        /* See Intel's "Fast SHA-256 Implementations" for the ROR transformation */ \
        movl  %a, %eax;            \
        rorl  $9, %eax;            \
        xorl  %a, %eax;            \
        rorl  $11, %eax;           \
        xorl  %a, %eax;            \
        rorl  $2, %eax;            \
        movl  %c, %ecx;            \
        addl  %eax, %h;            \
        movl  %c, %eax;            \
        orl   %b, %eax;            \
        andl  %b, %ecx;            \
        andl  %a, %eax;            \
        orl   %ecx, %eax;          \
        addl  %eax, %h;
    
    /* Save registers, allocate scratch space */
    movq  %r10, %xmm0
    movq  %r11, %xmm1
    movq  %r12, %xmm2
    movq  %r13, %xmm3
    movq  %r14, %xmm4
    movq  %r15, %xmm5
    movq  %rbx, %xmm6
    subq  $64, %rsp
    
    /* Load state */
    movl   0(%rdi), %r8d   /* a */
    movl   4(%rdi), %r9d   /* b */
    movl   8(%rdi), %r10d  /* c */
    movl  12(%rdi), %r11d  /* d */
    movl  16(%rdi), %r12d  /* e */
    movl  20(%rdi), %r13d  /* f */
    movl  24(%rdi), %r14d  /* g */
    movl  28(%rdi), %r15d  /* h */
    
    /* Do 64 rounds of hashing */
    ROUNDa( 0, r8d , r9d , r10d, r11d, r12d, r13d, r14d, r15d,  0x428A2F98)
    ROUNDa( 1, r15d, r8d , r9d , r10d, r11d, r12d, r13d, r14d,  0x71374491)
    ROUNDa( 2, r14d, r15d, r8d , r9d , r10d, r11d, r12d, r13d, -0x4A3F0431)
    ROUNDa( 3, r13d, r14d, r15d, r8d , r9d , r10d, r11d, r12d, -0x164A245B)
    ROUNDa( 4, r12d, r13d, r14d, r15d, r8d , r9d , r10d, r11d,  0x3956C25B)
    ROUNDa( 5, r11d, r12d, r13d, r14d, r15d, r8d , r9d , r10d,  0x59F111F1)
    ROUNDa( 6, r10d, r11d, r12d, r13d, r14d, r15d, r8d , r9d , -0x6DC07D5C)
    ROUNDa( 7, r9d , r10d, r11d, r12d, r13d, r14d, r15d, r8d , -0x54E3A12B)
    ROUNDa( 8, r8d , r9d , r10d, r11d, r12d, r13d, r14d, r15d, -0x27F85568)
    ROUNDa( 9, r15d, r8d , r9d , r10d, r11d, r12d, r13d, r14d,  0x12835B01)
    ROUNDa(10, r14d, r15d, r8d , r9d , r10d, r11d, r12d, r13d,  0x243185BE)
    ROUNDa(11, r13d, r14d, r15d, r8d , r9d , r10d, r11d, r12d,  0x550C7DC3)
    ROUNDa(12, r12d, r13d, r14d, r15d, r8d , r9d , r10d, r11d,  0x72BE5D74)
    ROUNDa(13, r11d, r12d, r13d, r14d, r15d, r8d , r9d , r10d, -0x7F214E02)
    ROUNDa(14, r10d, r11d, r12d, r13d, r14d, r15d, r8d , r9d , -0x6423F959)
    ROUNDa(15, r9d , r10d, r11d, r12d, r13d, r14d, r15d, r8d , -0x3E640E8C)
    ROUNDb(16, r8d , r9d , r10d, r11d, r12d, r13d, r14d, r15d, -0x1B64963F)
    ROUNDb(17, r15d, r8d , r9d , r10d, r11d, r12d, r13d, r14d, -0x1041B87A)
    ROUNDb(18, r14d, r15d, r8d , r9d , r10d, r11d, r12d, r13d,  0x0FC19DC6)
    ROUNDb(19, r13d, r14d, r15d, r8d , r9d , r10d, r11d, r12d,  0x240CA1CC)
    ROUNDb(20, r12d, r13d, r14d, r15d, r8d , r9d , r10d, r11d,  0x2DE92C6F)
    ROUNDb(21, r11d, r12d, r13d, r14d, r15d, r8d , r9d , r10d,  0x4A7484AA)
    ROUNDb(22, r10d, r11d, r12d, r13d, r14d, r15d, r8d , r9d ,  0x5CB0A9DC)
    ROUNDb(23, r9d , r10d, r11d, r12d, r13d, r14d, r15d, r8d ,  0x76F988DA)
    ROUNDb(24, r8d , r9d , r10d, r11d, r12d, r13d, r14d, r15d, -0x67C1AEAE)
    ROUNDb(25, r15d, r8d , r9d , r10d, r11d, r12d, r13d, r14d, -0x57CE3993)
    ROUNDb(26, r14d, r15d, r8d , r9d , r10d, r11d, r12d, r13d, -0x4FFCD838)
    ROUNDb(27, r13d, r14d, r15d, r8d , r9d , r10d, r11d, r12d, -0x40A68039)
    ROUNDb(28, r12d, r13d, r14d, r15d, r8d , r9d , r10d, r11d, -0x391FF40D)
    ROUNDb(29, r11d, r12d, r13d, r14d, r15d, r8d , r9d , r10d, -0x2A586EB9)
    ROUNDb(30, r10d, r11d, r12d, r13d, r14d, r15d, r8d , r9d ,  0x06CA6351)
    ROUNDb(31, r9d , r10d, r11d, r12d, r13d, r14d, r15d, r8d ,  0x14292967)
    ROUNDb(32, r8d , r9d , r10d, r11d, r12d, r13d, r14d, r15d,  0x27B70A85)
    ROUNDb(33, r15d, r8d , r9d , r10d, r11d, r12d, r13d, r14d,  0x2E1B2138)
    ROUNDb(34, r14d, r15d, r8d , r9d , r10d, r11d, r12d, r13d,  0x4D2C6DFC)
    ROUNDb(35, r13d, r14d, r15d, r8d , r9d , r10d, r11d, r12d,  0x53380D13)
    ROUNDb(36, r12d, r13d, r14d, r15d, r8d , r9d , r10d, r11d,  0x650A7354)
    ROUNDb(37, r11d, r12d, r13d, r14d, r15d, r8d , r9d , r10d,  0x766A0ABB)
    ROUNDb(38, r10d, r11d, r12d, r13d, r14d, r15d, r8d , r9d , -0x7E3D36D2)
    ROUNDb(39, r9d , r10d, r11d, r12d, r13d, r14d, r15d, r8d , -0x6D8DD37B)
    ROUNDb(40, r8d , r9d , r10d, r11d, r12d, r13d, r14d, r15d, -0x5D40175F)
    ROUNDb(41, r15d, r8d , r9d , r10d, r11d, r12d, r13d, r14d, -0x57E599B5)
    ROUNDb(42, r14d, r15d, r8d , r9d , r10d, r11d, r12d, r13d, -0x3DB47490)
    ROUNDb(43, r13d, r14d, r15d, r8d , r9d , r10d, r11d, r12d, -0x3893AE5D)
    ROUNDb(44, r12d, r13d, r14d, r15d, r8d , r9d , r10d, r11d, -0x2E6D17E7)
    ROUNDb(45, r11d, r12d, r13d, r14d, r15d, r8d , r9d , r10d, -0x2966F9DC)
    ROUNDb(46, r10d, r11d, r12d, r13d, r14d, r15d, r8d , r9d , -0x0BF1CA7B)
    ROUNDb(47, r9d , r10d, r11d, r12d, r13d, r14d, r15d, r8d ,  0x106AA070)
    ROUNDb(48, r8d , r9d , r10d, r11d, r12d, r13d, r14d, r15d,  0x19A4C116)
    ROUNDb(49, r15d, r8d , r9d , r10d, r11d, r12d, r13d, r14d,  0x1E376C08)
    ROUNDb(50, r14d, r15d, r8d , r9d , r10d, r11d, r12d, r13d,  0x2748774C)
    ROUNDb(51, r13d, r14d, r15d, r8d , r9d , r10d, r11d, r12d,  0x34B0BCB5)
    ROUNDb(52, r12d, r13d, r14d, r15d, r8d , r9d , r10d, r11d,  0x391C0CB3)
    ROUNDb(53, r11d, r12d, r13d, r14d, r15d, r8d , r9d , r10d,  0x4ED8AA4A)
    ROUNDb(54, r10d, r11d, r12d, r13d, r14d, r15d, r8d , r9d ,  0x5B9CCA4F)
    ROUNDb(55, r9d , r10d, r11d, r12d, r13d, r14d, r15d, r8d ,  0x682E6FF3)
    ROUNDb(56, r8d , r9d , r10d, r11d, r12d, r13d, r14d, r15d,  0x748F82EE)
    ROUNDb(57, r15d, r8d , r9d , r10d, r11d, r12d, r13d, r14d,  0x78A5636F)
    ROUNDb(58, r14d, r15d, r8d , r9d , r10d, r11d, r12d, r13d, -0x7B3787EC)
    ROUNDb(59, r13d, r14d, r15d, r8d , r9d , r10d, r11d, r12d, -0x7338FDF8)
    ROUNDb(60, r12d, r13d, r14d, r15d, r8d , r9d , r10d, r11d, -0x6F410006)
    ROUNDb(61, r11d, r12d, r13d, r14d, r15d, r8d , r9d , r10d, -0x5BAF9315)
    ROUNDb(62, r10d, r11d, r12d, r13d, r14d, r15d, r8d , r9d , -0x41065C09)
    ROUNDb(63, r9d , r10d, r11d, r12d, r13d, r14d, r15d, r8d , -0x398E870E)
    
    /* Add to state */
    addl  %r8d ,  0(%rdi)
    addl  %r9d ,  4(%rdi)
    addl  %r10d,  8(%rdi)
    addl  %r11d, 12(%rdi)
    addl  %r12d, 16(%rdi)
    addl  %r13d, 20(%rdi)
    addl  %r14d, 24(%rdi)
    addl  %r15d, 28(%rdi)
    
    /* Restore registers */
    movq  %xmm0, %r10
    movq  %xmm1, %r11
    movq  %xmm2, %r12
    movq  %xmm3, %r13
    movq  %xmm4, %r14
    movq  %xmm5, %r15
    movq  %xmm6, %rbx
    addq  $64, %rsp
    retq
