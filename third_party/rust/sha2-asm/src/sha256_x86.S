/* 
 * SHA-256 hash in x86 assembly
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


/* void sha256_compress(uint32_t state[8], const uint8_t block[64]) */
#if defined(__APPLE__) || defined(_WIN32)
.globl _sha256_compress
_sha256_compress:
#else
.globl sha256_compress
sha256_compress:
#endif
    /* 
     * Storage usage:
     *   Bytes  Location   Description
     *       4  eax        Temporary for calculation per round
     *       4  ebx        Temporary for calculation per round
     *       4  ecx        Temporary for calculation per round
     *       4  edx        Temporary for calculation per round
     *       4  ebp        Temporary for calculation per round
     *       4  esi        (During state loading and update) base address of state array argument
     *                     (During hash rounds) temporary for calculation per round
     *       4  edi        Base address of block array argument (during key schedule loading rounds only)
     *       4  esp        x86 stack pointer
     *      32  [esp+  0]  SHA-256 state variables A,B,C,D,E,F,G,H (4 bytes each)
     *      64  [esp+ 32]  Key schedule of 16 * 4 bytes
     *       4  [esp+ 96]  Caller's value of ebx
     *       4  [esp+100]  Caller's value of esi
     *       4  [esp+104]  Caller's value of edi
     *       4  [esp+108]  Caller's value of ebp
     */
    
    #define SCHED(i)  ((((i)&0xF)+8)*4)(%esp)
    
    #define ROUNDa(i, a, b, c, d, e, f, g, h, k)  \
        movl    (i*4)(%edi), %ebp;  \
        bswapl  %ebp;               \
        movl    %ebp, SCHED(i);     \
        ROUNDTAIL(i, a, b, c, d, e, f, g, h, k)
    
    #define ROUNDb(i, a, b, c, d, e, f, g, h, k)  \
        movl  SCHED(i-15), %eax;  \
        movl  SCHED(i-16), %ebp;  \
        movl  %eax, %ebx;         \
        addl  SCHED(i- 7), %ebp;  \
        movl  %eax, %ecx;         \
        rorl  $18, %ebx;          \
        shrl  $3, %ecx;           \
        rorl  $7, %eax;           \
        xorl  %ecx, %ebx;         \
        xorl  %ebx, %eax;         \
        addl  %eax, %ebp;         \
        movl  SCHED(i- 2), %eax;  \
        movl  %eax, %ebx;         \
        movl  %eax, %ecx;         \
        rorl  $19, %ebx;          \
        shrl  $10, %ecx;          \
        rorl  $17, %eax;          \
        xorl  %ecx, %ebx;         \
        xorl  %ebx, %eax;         \
        addl  %eax, %ebp;         \
        movl  %ebp, SCHED(i);     \
        ROUNDTAIL(i, a, b, c, d, e, f, g, h, k)
    
    #define STATE(i)  (i*4)(%esp)
    
    #define ROUNDTAIL(i, a, b, c, d, e, f, g, h, k)  \
        /* Part 0 */               \
        movl  STATE(e), %eax;      \
        movl  %eax, %ebx;          \
        movl  %eax, %ecx;          \
        movl  %eax, %edx;          \
        rorl  $11, %eax;           \
        rorl  $25, %ebx;           \
        rorl  $6, %ecx;            \
        movl  STATE(h), %esi;      \
        xorl  %ebx, %eax;          \
        xorl  %eax, %ecx;          \
        addl  %ebp, %esi;          \
        movl  STATE(g), %ebx;      \
        movl  STATE(f), %eax;      \
        xorl  %ebx, %eax;          \
        andl  %edx, %eax;          \
        xorl  %ebx, %eax;          \
        leal  k(%ecx,%eax), %ecx;  \
        addl  %ecx, %esi;          \
        /* Part 1 */               \
        addl  %esi, STATE(d);      \
        /* Part 2 */               \
        movl  STATE(a), %eax;      \
        movl  %eax, %ebx;          \
        movl  %eax, %ecx;          \
        movl  %eax, %edx;          \
        rorl  $13, %eax;           \
        rorl  $22, %ebx;           \
        rorl  $2, %ecx;            \
        xorl  %ebx, %eax;          \
        xorl  %eax, %ecx;          \
        movl  STATE(c), %eax;      \
        addl  %ecx, %esi;          \
        movl  %eax, %ecx;          \
        movl  STATE(b), %ebx;      \
        orl   %ebx, %ecx;          \
        andl  %ebx, %eax;          \
        andl  %edx, %ecx;          \
        orl   %eax, %ecx;          \
        addl  %ecx, %esi;          \
        movl  %esi, STATE(h);
    
    /* Allocate scratch space, save registers */
    subl  $112, %esp
    movl  %ebx,  96(%esp)
    movl  %esi, 100(%esp)
    movl  %edi, 104(%esp)
    movl  %ebp, 108(%esp)
    
    /* Copy state */
    movl  116(%esp), %esi  /* Argument: state */
    movl   0(%esi), %eax;  movl %eax,  0(%esp)
    movl   4(%esi), %eax;  movl %eax,  4(%esp)
    movl   8(%esi), %eax;  movl %eax,  8(%esp)
    movl  12(%esi), %eax;  movl %eax, 12(%esp)
    movl  16(%esi), %eax;  movl %eax, 16(%esp)
    movl  20(%esi), %eax;  movl %eax, 20(%esp)
    movl  24(%esi), %eax;  movl %eax, 24(%esp)
    movl  28(%esi), %eax;  movl %eax, 28(%esp)
    
    /* Do 64 rounds of hashing */
    movl    120(%esp), %edi  /* Argument: block */
    ROUNDa( 0, 0, 1, 2, 3, 4, 5, 6, 7, 0x428A2F98)
    ROUNDa( 1, 7, 0, 1, 2, 3, 4, 5, 6, 0x71374491)
    ROUNDa( 2, 6, 7, 0, 1, 2, 3, 4, 5, 0xB5C0FBCF)
    ROUNDa( 3, 5, 6, 7, 0, 1, 2, 3, 4, 0xE9B5DBA5)
    ROUNDa( 4, 4, 5, 6, 7, 0, 1, 2, 3, 0x3956C25B)
    ROUNDa( 5, 3, 4, 5, 6, 7, 0, 1, 2, 0x59F111F1)
    ROUNDa( 6, 2, 3, 4, 5, 6, 7, 0, 1, 0x923F82A4)
    ROUNDa( 7, 1, 2, 3, 4, 5, 6, 7, 0, 0xAB1C5ED5)
    ROUNDa( 8, 0, 1, 2, 3, 4, 5, 6, 7, 0xD807AA98)
    ROUNDa( 9, 7, 0, 1, 2, 3, 4, 5, 6, 0x12835B01)
    ROUNDa(10, 6, 7, 0, 1, 2, 3, 4, 5, 0x243185BE)
    ROUNDa(11, 5, 6, 7, 0, 1, 2, 3, 4, 0x550C7DC3)
    ROUNDa(12, 4, 5, 6, 7, 0, 1, 2, 3, 0x72BE5D74)
    ROUNDa(13, 3, 4, 5, 6, 7, 0, 1, 2, 0x80DEB1FE)
    ROUNDa(14, 2, 3, 4, 5, 6, 7, 0, 1, 0x9BDC06A7)
    ROUNDa(15, 1, 2, 3, 4, 5, 6, 7, 0, 0xC19BF174)
    ROUNDb(16, 0, 1, 2, 3, 4, 5, 6, 7, 0xE49B69C1)
    ROUNDb(17, 7, 0, 1, 2, 3, 4, 5, 6, 0xEFBE4786)
    ROUNDb(18, 6, 7, 0, 1, 2, 3, 4, 5, 0x0FC19DC6)
    ROUNDb(19, 5, 6, 7, 0, 1, 2, 3, 4, 0x240CA1CC)
    ROUNDb(20, 4, 5, 6, 7, 0, 1, 2, 3, 0x2DE92C6F)
    ROUNDb(21, 3, 4, 5, 6, 7, 0, 1, 2, 0x4A7484AA)
    ROUNDb(22, 2, 3, 4, 5, 6, 7, 0, 1, 0x5CB0A9DC)
    ROUNDb(23, 1, 2, 3, 4, 5, 6, 7, 0, 0x76F988DA)
    ROUNDb(24, 0, 1, 2, 3, 4, 5, 6, 7, 0x983E5152)
    ROUNDb(25, 7, 0, 1, 2, 3, 4, 5, 6, 0xA831C66D)
    ROUNDb(26, 6, 7, 0, 1, 2, 3, 4, 5, 0xB00327C8)
    ROUNDb(27, 5, 6, 7, 0, 1, 2, 3, 4, 0xBF597FC7)
    ROUNDb(28, 4, 5, 6, 7, 0, 1, 2, 3, 0xC6E00BF3)
    ROUNDb(29, 3, 4, 5, 6, 7, 0, 1, 2, 0xD5A79147)
    ROUNDb(30, 2, 3, 4, 5, 6, 7, 0, 1, 0x06CA6351)
    ROUNDb(31, 1, 2, 3, 4, 5, 6, 7, 0, 0x14292967)
    ROUNDb(32, 0, 1, 2, 3, 4, 5, 6, 7, 0x27B70A85)
    ROUNDb(33, 7, 0, 1, 2, 3, 4, 5, 6, 0x2E1B2138)
    ROUNDb(34, 6, 7, 0, 1, 2, 3, 4, 5, 0x4D2C6DFC)
    ROUNDb(35, 5, 6, 7, 0, 1, 2, 3, 4, 0x53380D13)
    ROUNDb(36, 4, 5, 6, 7, 0, 1, 2, 3, 0x650A7354)
    ROUNDb(37, 3, 4, 5, 6, 7, 0, 1, 2, 0x766A0ABB)
    ROUNDb(38, 2, 3, 4, 5, 6, 7, 0, 1, 0x81C2C92E)
    ROUNDb(39, 1, 2, 3, 4, 5, 6, 7, 0, 0x92722C85)
    ROUNDb(40, 0, 1, 2, 3, 4, 5, 6, 7, 0xA2BFE8A1)
    ROUNDb(41, 7, 0, 1, 2, 3, 4, 5, 6, 0xA81A664B)
    ROUNDb(42, 6, 7, 0, 1, 2, 3, 4, 5, 0xC24B8B70)
    ROUNDb(43, 5, 6, 7, 0, 1, 2, 3, 4, 0xC76C51A3)
    ROUNDb(44, 4, 5, 6, 7, 0, 1, 2, 3, 0xD192E819)
    ROUNDb(45, 3, 4, 5, 6, 7, 0, 1, 2, 0xD6990624)
    ROUNDb(46, 2, 3, 4, 5, 6, 7, 0, 1, 0xF40E3585)
    ROUNDb(47, 1, 2, 3, 4, 5, 6, 7, 0, 0x106AA070)
    ROUNDb(48, 0, 1, 2, 3, 4, 5, 6, 7, 0x19A4C116)
    ROUNDb(49, 7, 0, 1, 2, 3, 4, 5, 6, 0x1E376C08)
    ROUNDb(50, 6, 7, 0, 1, 2, 3, 4, 5, 0x2748774C)
    ROUNDb(51, 5, 6, 7, 0, 1, 2, 3, 4, 0x34B0BCB5)
    ROUNDb(52, 4, 5, 6, 7, 0, 1, 2, 3, 0x391C0CB3)
    ROUNDb(53, 3, 4, 5, 6, 7, 0, 1, 2, 0x4ED8AA4A)
    ROUNDb(54, 2, 3, 4, 5, 6, 7, 0, 1, 0x5B9CCA4F)
    ROUNDb(55, 1, 2, 3, 4, 5, 6, 7, 0, 0x682E6FF3)
    ROUNDb(56, 0, 1, 2, 3, 4, 5, 6, 7, 0x748F82EE)
    ROUNDb(57, 7, 0, 1, 2, 3, 4, 5, 6, 0x78A5636F)
    ROUNDb(58, 6, 7, 0, 1, 2, 3, 4, 5, 0x84C87814)
    ROUNDb(59, 5, 6, 7, 0, 1, 2, 3, 4, 0x8CC70208)
    ROUNDb(60, 4, 5, 6, 7, 0, 1, 2, 3, 0x90BEFFFA)
    ROUNDb(61, 3, 4, 5, 6, 7, 0, 1, 2, 0xA4506CEB)
    ROUNDb(62, 2, 3, 4, 5, 6, 7, 0, 1, 0xBEF9A3F7)
    ROUNDb(63, 1, 2, 3, 4, 5, 6, 7, 0, 0xC67178F2)
    
    /* Add to state */
    movl  116(%esp), %esi  /* Argument: state */
    movl   0(%esp), %eax;  addl %eax,  0(%esi)
    movl   4(%esp), %eax;  addl %eax,  4(%esi)
    movl   8(%esp), %eax;  addl %eax,  8(%esi)
    movl  12(%esp), %eax;  addl %eax, 12(%esi)
    movl  16(%esp), %eax;  addl %eax, 16(%esi)
    movl  20(%esp), %eax;  addl %eax, 20(%esi)
    movl  24(%esp), %eax;  addl %eax, 24(%esi)
    movl  28(%esp), %eax;  addl %eax, 28(%esi)
    
    /* Restore registers */
    movl   96(%esp), %ebx
    movl  100(%esp), %esi
    movl  104(%esp), %edi
    movl  108(%esp), %ebp
    addl  $112, %esp
    retl
