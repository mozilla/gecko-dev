// |jit-test| skip-if: !wasmSimdEnabled()

/* Copyright 2021 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// ./test/core/simd/simd_align.wast

// ./test/core/simd/simd_align.wast:3
let $0 = instantiate(`(module (memory 1) (func (drop (v128.load align=1 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:4
let $1 = instantiate(`(module (memory 1) (func (drop (v128.load align=2 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:5
let $2 = instantiate(`(module (memory 1) (func (drop (v128.load align=4 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:6
let $3 = instantiate(`(module (memory 1) (func (drop (v128.load align=8 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:7
let $4 = instantiate(`(module (memory 1) (func (drop (v128.load align=16 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:9
let $5 = instantiate(`(module (memory 1) (func (v128.store align=1 (i32.const 0) (v128.const i32x4 0 1 2 3))))`);

// ./test/core/simd/simd_align.wast:10
let $6 = instantiate(`(module (memory 1) (func (v128.store align=2 (i32.const 0) (v128.const i32x4 0 1 2 3))))`);

// ./test/core/simd/simd_align.wast:11
let $7 = instantiate(`(module (memory 1) (func (v128.store align=4 (i32.const 0) (v128.const i32x4 0 1 2 3))))`);

// ./test/core/simd/simd_align.wast:12
let $8 = instantiate(`(module (memory 1) (func (v128.store align=8 (i32.const 0) (v128.const i32x4 0 1 2 3))))`);

// ./test/core/simd/simd_align.wast:13
let $9 = instantiate(`(module (memory 1) (func (v128.store align=16 (i32.const 0) (v128.const i32x4 0 1 2 3))))`);

// ./test/core/simd/simd_align.wast:15
let $10 = instantiate(`(module (memory 1) (func (drop (v128.load8x8_s align=1 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:16
let $11 = instantiate(`(module (memory 1) (func (drop (v128.load8x8_s align=2 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:17
let $12 = instantiate(`(module (memory 1) (func (drop (v128.load8x8_s align=4 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:18
let $13 = instantiate(`(module (memory 1) (func (drop (v128.load8x8_s align=8 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:19
let $14 = instantiate(`(module (memory 1) (func (drop (v128.load8x8_u align=1 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:20
let $15 = instantiate(`(module (memory 1) (func (drop (v128.load8x8_u align=2 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:21
let $16 = instantiate(`(module (memory 1) (func (drop (v128.load8x8_u align=4 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:22
let $17 = instantiate(`(module (memory 1) (func (drop (v128.load8x8_u align=8 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:23
let $18 = instantiate(`(module (memory 1) (func (drop (v128.load16x4_s align=1 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:24
let $19 = instantiate(`(module (memory 1) (func (drop (v128.load16x4_s align=2 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:25
let $20 = instantiate(`(module (memory 1) (func (drop (v128.load16x4_s align=4 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:26
let $21 = instantiate(`(module (memory 1) (func (drop (v128.load16x4_s align=8 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:27
let $22 = instantiate(`(module (memory 1) (func (drop (v128.load16x4_u align=1 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:28
let $23 = instantiate(`(module (memory 1) (func (drop (v128.load16x4_u align=2 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:29
let $24 = instantiate(`(module (memory 1) (func (drop (v128.load16x4_u align=4 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:30
let $25 = instantiate(`(module (memory 1) (func (drop (v128.load16x4_u align=8 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:31
let $26 = instantiate(`(module (memory 1) (func (drop (v128.load32x2_s align=1 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:32
let $27 = instantiate(`(module (memory 1) (func (drop (v128.load32x2_s align=2 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:33
let $28 = instantiate(`(module (memory 1) (func (drop (v128.load32x2_s align=4 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:34
let $29 = instantiate(`(module (memory 1) (func (drop (v128.load32x2_s align=8 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:35
let $30 = instantiate(`(module (memory 1) (func (drop (v128.load32x2_u align=1 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:36
let $31 = instantiate(`(module (memory 1) (func (drop (v128.load32x2_u align=2 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:37
let $32 = instantiate(`(module (memory 1) (func (drop (v128.load32x2_u align=4 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:38
let $33 = instantiate(`(module (memory 1) (func (drop (v128.load32x2_u align=8 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:40
let $34 = instantiate(`(module (memory 1) (func (drop (v128.load8_splat align=1 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:41
let $35 = instantiate(`(module (memory 1) (func (drop (v128.load16_splat align=1 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:42
let $36 = instantiate(`(module (memory 1) (func (drop (v128.load16_splat align=2 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:43
let $37 = instantiate(`(module (memory 1) (func (drop (v128.load32_splat align=1 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:44
let $38 = instantiate(`(module (memory 1) (func (drop (v128.load32_splat align=2 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:45
let $39 = instantiate(`(module (memory 1) (func (drop (v128.load32_splat align=4 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:46
let $40 = instantiate(`(module (memory 1) (func (drop (v128.load64_splat align=1 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:47
let $41 = instantiate(`(module (memory 1) (func (drop (v128.load64_splat align=2 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:48
let $42 = instantiate(`(module (memory 1) (func (drop (v128.load64_splat align=4 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:49
let $43 = instantiate(`(module (memory 1) (func (drop (v128.load64_splat align=8 (i32.const 0)))))`);

// ./test/core/simd/simd_align.wast:53
assert_invalid(
  () => instantiate(`(module (memory 1) (func (drop (v128.load align=32 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/simd/simd_align.wast:57
assert_invalid(
  () => instantiate(`(module (memory 0) (func(v128.store align=32 (i32.const 0) (v128.const i32x4 0 0 0 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/simd/simd_align.wast:61
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result v128) (v128.load8x8_s align=16 (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/simd/simd_align.wast:65
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result v128) (v128.load8x8_u align=16 (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/simd/simd_align.wast:69
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result v128) (v128.load16x4_s align=16 (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/simd/simd_align.wast:73
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result v128) (v128.load16x4_u align=16 (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/simd/simd_align.wast:77
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result v128) (v128.load32x2_s align=16 (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/simd/simd_align.wast:81
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result v128) (v128.load32x2_u align=16 (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/simd/simd_align.wast:85
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result v128) (v128.load8_splat align=2 (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/simd/simd_align.wast:89
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result v128) (v128.load16_splat align=4 (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/simd/simd_align.wast:93
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result v128) (v128.load32_splat align=8 (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/simd/simd_align.wast:97
assert_invalid(
  () => instantiate(`(module (memory 1) (func (result v128) (v128.load64_splat align=16 (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/simd/simd_align.wast:104
assert_malformed(
  () => instantiate(`(memory 1) (func (drop (v128.load align=-1 (i32.const 0)))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_align.wast:110
assert_malformed(
  () => instantiate(`(memory 1) (func (drop (v128.load align=0 (i32.const 0)))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:116
assert_malformed(
  () => instantiate(`(memory 1) (func (drop (v128.load align=7 (i32.const 0)))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:122
assert_malformed(
  () => instantiate(`(memory 1) (func (v128.store align=-1 (i32.const 0) (v128.const i32x4 0 0 0 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_align.wast:128
assert_malformed(
  () => instantiate(`(memory 0) (func (v128.store align=0 (i32.const 0) (v128.const i32x4 0 0 0 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:134
assert_malformed(
  () => instantiate(`(memory 0) (func (v128.store align=7 (i32.const 0) (v128.const i32x4 0 0 0 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:140
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load8x8_s align=-1 (i32.const 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_align.wast:146
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load8x8_s align=0 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:152
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load8x8_s align=7 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:158
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load8x8_u align=-1 (i32.const 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_align.wast:164
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load8x8_u align=0 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:170
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load8x8_u align=7 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:176
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load16x4_s align=-1 (i32.const 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_align.wast:182
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load16x4_s align=0 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:188
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load16x4_s align=7 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:194
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load16x4_u align=-1 (i32.const 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_align.wast:200
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load16x4_u align=0 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:206
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load16x4_u align=7 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:212
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load32x2_s align=-1 (i32.const 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_align.wast:218
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load32x2_s align=0 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:224
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load32x2_s align=7 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:230
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load32x2_u align=-1 (i32.const 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_align.wast:236
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load32x2_u align=0 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:242
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load32x2_u align=7 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:248
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load8_splat align=-1 (i32.const 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_align.wast:254
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load8_splat align=0 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:260
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load16_splat align=-1 (i32.const 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_align.wast:266
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load16_splat align=0 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:272
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load32_splat align=-1 (i32.const 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_align.wast:278
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load32_splat align=0 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:284
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load32_splat align=3 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:290
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load64_splat align=-1 (i32.const 0))) `),
  `unknown operator`,
);

// ./test/core/simd/simd_align.wast:296
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load64_splat align=0 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:302
assert_malformed(
  () => instantiate(`(memory 1) (func (result v128) (v128.load64_splat align=7 (i32.const 0))) `),
  `alignment must be a power of two`,
);

// ./test/core/simd/simd_align.wast:311
let $44 = instantiate(`(module
  (memory 1 1)
  (func (export "v128.load align=16") (param $$address i32) (result v128)
    (v128.load align=16 (local.get $$address))
  )
  (func (export "v128.store align=16") (param $$address i32) (param $$value v128)
    (v128.store align=16 (local.get $$address) (local.get $$value))
  )
)`);

// ./test/core/simd/simd_align.wast:321
assert_return(() => invoke($44, `v128.load align=16`, [0]), [i32x4([0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_align.wast:322
assert_return(() => invoke($44, `v128.load align=16`, [1]), [i32x4([0x0, 0x0, 0x0, 0x0])]);

// ./test/core/simd/simd_align.wast:323
assert_return(
  () => invoke($44, `v128.store align=16`, [
    1,
    i8x16([0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10]),
  ]),
  [],
);

// ./test/core/simd/simd_align.wast:324
assert_return(
  () => invoke($44, `v128.load align=16`, [0]),
  [
    i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]),
  ],
);

// ./test/core/simd/simd_align.wast:328
let $45 = instantiate(`(module
  (memory 1)
  (func (export "v128_unaligned_read_and_write") (result v128)
    (local v128)
    (v128.store (i32.const 0) (v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15))
    (v128.load (i32.const 0))
  )
  (func (export "v128_aligned_read_and_write") (result v128)
    (local v128)
    (v128.store align=2 (i32.const 0) (v128.const i16x8 0 1 2 3 4 5 6 7))
    (v128.load align=2  (i32.const 0))
  )
  (func (export "v128_aligned_read_and_unaligned_write") (result v128)
    (local v128)
    (v128.store (i32.const 0) (v128.const i32x4 0 1 2 3))
    (v128.load align=2 (i32.const 0))
  )
  (func (export "v128_unaligned_read_and_aligned_write") (result v128)
    (local v128)
    (v128.store align=2 (i32.const 0) (v128.const i32x4 0 1 2 3))
    (v128.load (i32.const 0))
  )
)`);

// ./test/core/simd/simd_align.wast:352
assert_return(
  () => invoke($45, `v128_unaligned_read_and_write`, []),
  [
    i8x16([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf]),
  ],
);

// ./test/core/simd/simd_align.wast:353
assert_return(
  () => invoke($45, `v128_aligned_read_and_write`, []),
  [i16x8([0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7])],
);

// ./test/core/simd/simd_align.wast:354
assert_return(
  () => invoke($45, `v128_aligned_read_and_unaligned_write`, []),
  [i32x4([0x0, 0x1, 0x2, 0x3])],
);

// ./test/core/simd/simd_align.wast:355
assert_return(
  () => invoke($45, `v128_unaligned_read_and_aligned_write`, []),
  [i32x4([0x0, 0x1, 0x2, 0x3])],
);
