/*
 * SHA-256 hash in AArch64 assembly
 *
 * Copyright (c) 2020 Emmanuel Gil Peyrot <linkmauve@linkmauve.fr>. (MIT License)
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
.global sha256_compress
sha256_compress:
	/*
	 * Storage usage:
	 *   Bytes  Location  Description
	 *       4  x0        state argument
	 *       4  x1        block argument
	 *       4  x2        pointer to k
	 *      16  q0        state0
	 *      16  q1        state1
	 *      16  q2        abef
	 *      16  q3        cdgh
	 *      16  q4        k0
	 *      16  q5        k1
	 *      16  q8        W0
	 *      16  q9        W1
	 *      16  q10       W2
	 *      16  q11       W3
	 */

	// save the lower half of q8-q11
	stp d8,  d9, [sp,#-32]!
	stp d10, d11, [sp,#16]

	// Load state in registers
	ldp       q0, q1, [x0]
	mov       v2.16b, v0.16b
	mov       v3.16b, v1.16b

	// Load block in registers
	ld1       {v8.4s-v11.4s}, [x1]

	// TODO: only do that on little endian
	rev32     v8.16b,  v8.16b
	rev32     v9.16b,  v9.16b
	rev32     v10.16b, v10.16b
	rev32     v11.16b, v11.16b

	// Compute the pointer to k
	adrp      x2, .K
	add       x2, x2, :lo12:.K

	// load k
	ld1       {v16.4s-v19.4s}, [x2], #64
	ld1       {v20.4s-v23.4s}, [x2], #64
	ld1       {v24.4s-v27.4s}, [x2], #64
	ld1       {v28.4s-v31.4s}, [x2]
	add       v6.4s, v8.4s, v16.4s

	// Rounds 0-3
	sha256su0 v8.4s, v9.4s
	mov       v4.16b, v2.16b
	add       v7.4s, v9.4s, v17.4s
	sha256h   q2, q3, v6.4s
	sha256h2  q3, q4, v6.4s
	sha256su1 v8.4s, v10.4s, v11.4s

	// Rounds 4-7
	sha256su0 v9.4s, v10.4s
	mov       v4.16b, v2.16b
	add       v6.4s, v10.4s, v18.4s
	sha256h   q2, q3, v7.4s
	sha256h2  q3, q4, v7.4s
	sha256su1 v9.4s, v11.4s, v8.4s

	// Rounds 8-11
	sha256su0 v10.4s, v11.4s
	mov       v4.16b, v2.16b
	add       v7.4s, v11.4s, v19.4s
	sha256h   q2, q3, v6.4s
	sha256h2  q3, q4, v6.4s
	sha256su1 v10.4s, v8.4s, v9.4s

	// Rounds 12-15
	sha256su0 v11.4s, v8.4s
	mov       v4.16b, v2.16b
	add       v6.4s, v8.4s, v20.4s
	sha256h   q2, q3, v7.4s
	sha256h2  q3, q4, v7.4s
	sha256su1 v11.4s, v9.4s, v10.4s

	// Rounds 16-19
	sha256su0 v8.4s, v9.4s
	mov       v4.16b, v2.16b
	add       v7.4s, v9.4s, v21.4s
	sha256h   q2, q3, v6.4s
	sha256h2  q3, q4, v6.4s
	sha256su1 v8.4s, v10.4s, v11.4s

	// Rounds 20-23
	sha256su0 v9.4s, v10.4s
	mov       v4.16b, v2.16b
	add       v6.4s, v10.4s, v22.4s
	sha256h   q2, q3, v7.4s
	sha256h2  q3, q4, v7.4s
	sha256su1 v9.4s, v11.4s, v8.4s

	// Rounds 24-27
	sha256su0 v10.4s, v11.4s
	mov       v4.16b, v2.16b
	add       v7.4s, v11.4s, v23.4s
	sha256h   q2, q3, v6.4s
	sha256h2  q3, q4, v6.4s
	sha256su1 v10.4s, v8.4s, v9.4s

	// Rounds 28-31
	sha256su0 v11.4s, v8.4s
	mov       v4.16b, v2.16b
	add       v6.4s, v8.4s, v24.4s
	sha256h   q2, q3, v7.4s
	sha256h2  q3, q4, v7.4s
	sha256su1 v11.4s, v9.4s, v10.4s

	// Rounds 32-35
	sha256su0 v8.4s, v9.4s
	mov       v4.16b, v2.16b
	add       v7.4s, v9.4s, v25.4s
	sha256h   q2, q3, v6.4s
	sha256h2  q3, q4, v6.4s
	sha256su1 v8.4s, v10.4s, v11.4s

	// Rounds 36-39
	sha256su0 v9.4s, v10.4s
	mov       v4.16b, v2.16b
	add       v6.4s, v10.4s, v26.4s
	sha256h   q2, q3, v7.4s
	sha256h2  q3, q4, v7.4s
	sha256su1 v9.4s, v11.4s, v8.4s

	// Rounds 40-43
	sha256su0 v10.4s, v11.4s
	mov       v4.16b, v2.16b
	add       v7.4s, v11.4s, v27.4s
	sha256h   q2, q3, v6.4s
	sha256h2  q3, q4, v6.4s
	sha256su1 v10.4s, v8.4s, v9.4s

	// Rounds 44-47
	sha256su0 v11.4s, v8.4s
	mov       v4.16b, v2.16b
	add       v6.4s, v8.4s, v28.4s
	sha256h   q2, q3, v7.4s
	sha256h2  q3, q4, v7.4s
	sha256su1 v11.4s, v9.4s, v10.4s

	// Rounds 48-51
	mov       v4.16b, v2.16b
	add       v7.4s, v9.4s, v29.4s
	sha256h   q2, q3, v6.4s
	sha256h2  q3, q4, v6.4s

	// Rounds 52-55
	mov       v4.16b, v2.16b
	add       v6.4s, v10.4s, v30.4s
	sha256h   q2, q3, v7.4s
	sha256h2  q3, q4, v7.4s

	// Rounds 56-59
	mov       v4.16b, v2.16b
	add       v7.4s, v11.4s, v31.4s
	sha256h   q2, q3, v6.4s
	sha256h2  q3, q4, v6.4s

	// Rounds 60-63
	mov       v4.16b, v2.16b
	sha256h   q2, q3, v7.4s
	sha256h2  q3, q4, v7.4s

	// Update state
	add       v0.4s, v0.4s, v2.4s
	add       v1.4s, v1.4s, v3.4s
	stp       q0, q1, [x0]

	// restore
	ldp       d10, d11, [sp,#16]
	ldp       d8,  d9, [sp],#32
 
	ret
.align 4
.K:
	.word	0x428A2F98
	.word	0x71374491
	.word	0xB5C0FBCF
	.word	0xE9B5DBA5
	.word	0x3956C25B
	.word	0x59F111F1
	.word	0x923F82A4
	.word	0xAB1C5ED5
	.word	0xD807AA98
	.word	0x12835B01
	.word	0x243185BE
	.word	0x550C7DC3
	.word	0x72BE5D74
	.word	0x80DEB1FE
	.word	0x9BDC06A7
	.word	0xC19BF174
	.word	0xE49B69C1
	.word	0xEFBE4786
	.word	0x0FC19DC6
	.word	0x240CA1CC
	.word	0x2DE92C6F
	.word	0x4A7484AA
	.word	0x5CB0A9DC
	.word	0x76F988DA
	.word	0x983E5152
	.word	0xA831C66D
	.word	0xB00327C8
	.word	0xBF597FC7
	.word	0xC6E00BF3
	.word	0xD5A79147
	.word	0x06CA6351
	.word	0x14292967
	.word	0x27B70A85
	.word	0x2E1B2138
	.word	0x4D2C6DFC
	.word	0x53380D13
	.word	0x650A7354
	.word	0x766A0ABB
	.word	0x81C2C92E
	.word	0x92722C85
	.word	0xA2BFE8A1
	.word	0xA81A664B
	.word	0xC24B8B70
	.word	0xC76C51A3
	.word	0xD192E819
	.word	0xD6990624
	.word	0xF40E3585
	.word	0x106AA070
	.word	0x19A4C116
	.word	0x1E376C08
	.word	0x2748774C
	.word	0x34B0BCB5
	.word	0x391C0CB3
	.word	0x4ED8AA4A
	.word	0x5B9CCA4F
	.word	0x682E6FF3
	.word	0x748F82EE
	.word	0x78A5636F
	.word	0x84C87814
	.word	0x8CC70208
	.word	0x90BEFFFA
	.word	0xA4506CEB
	.word	0xBEF9A3F7
	.word	0xC67178F2
