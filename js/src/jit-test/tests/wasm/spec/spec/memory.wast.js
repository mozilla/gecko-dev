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

// ./test/core/memory.wast

// ./test/core/memory.wast:3
let $0 = instantiate(`(module (memory 0))`);

// ./test/core/memory.wast:4
let $1 = instantiate(`(module (memory 1))`);

// ./test/core/memory.wast:5
let $2 = instantiate(`(module (memory 0 0))`);

// ./test/core/memory.wast:6
let $3 = instantiate(`(module (memory 0 1))`);

// ./test/core/memory.wast:7
let $4 = instantiate(`(module (memory 1 256))`);

// ./test/core/memory.wast:8
let _anon_7 = module(`(module (memory 65536))`);

// ./test/core/memory.wast:9
let $5 = instantiate(`(module (memory 0 65536))`);

// ./test/core/memory.wast:11
let $6 = instantiate(`(module (memory (data)) (func (export "memsize") (result i32) (memory.size)))`);

// ./test/core/memory.wast:12
assert_return(() => invoke($6, `memsize`, []), [value("i32", 0)]);

// ./test/core/memory.wast:13
let $7 = instantiate(`(module (memory (data "")) (func (export "memsize") (result i32) (memory.size)))`);

// ./test/core/memory.wast:14
assert_return(() => invoke($7, `memsize`, []), [value("i32", 0)]);

// ./test/core/memory.wast:15
let $8 = instantiate(`(module (memory (data "x")) (func (export "memsize") (result i32) (memory.size)))`);

// ./test/core/memory.wast:16
assert_return(() => invoke($8, `memsize`, []), [value("i32", 1)]);

// ./test/core/memory.wast:18
assert_invalid(() => instantiate(`(module (data (i32.const 0)))`), `unknown memory`);

// ./test/core/memory.wast:19
assert_invalid(() => instantiate(`(module (data (i32.const 0) ""))`), `unknown memory`);

// ./test/core/memory.wast:20
assert_invalid(() => instantiate(`(module (data (i32.const 0) "x"))`), `unknown memory`);

// ./test/core/memory.wast:22
assert_invalid(
  () => instantiate(`(module (func (drop (f32.load (i32.const 0)))))`),
  `unknown memory`,
);

// ./test/core/memory.wast:26
assert_invalid(
  () => instantiate(`(module (func (f32.store (i32.const 0) (f32.const 0))))`),
  `unknown memory`,
);

// ./test/core/memory.wast:30
assert_invalid(
  () => instantiate(`(module (func (drop (i32.load8_s (i32.const 0)))))`),
  `unknown memory`,
);

// ./test/core/memory.wast:34
assert_invalid(
  () => instantiate(`(module (func (i32.store8 (i32.const 0) (i32.const 0))))`),
  `unknown memory`,
);

// ./test/core/memory.wast:38
assert_invalid(
  () => instantiate(`(module (func (drop (memory.size))))`),
  `unknown memory`,
);

// ./test/core/memory.wast:42
assert_invalid(
  () => instantiate(`(module (func (drop (memory.grow (i32.const 0)))))`),
  `unknown memory`,
);

// ./test/core/memory.wast:48
assert_invalid(
  () => instantiate(`(module (memory 1 0))`),
  `size minimum must not be greater than maximum`,
);

// ./test/core/memory.wast:52
assert_invalid(() => instantiate(`(module (memory 65537))`), `memory size`);

// ./test/core/memory.wast:56
assert_invalid(() => instantiate(`(module (memory 2147483648))`), `memory size`);

// ./test/core/memory.wast:60
assert_invalid(() => instantiate(`(module (memory 4294967295))`), `memory size`);

// ./test/core/memory.wast:64
assert_invalid(() => instantiate(`(module (memory 0 65537))`), `memory size`);

// ./test/core/memory.wast:68
assert_invalid(() => instantiate(`(module (memory 0 2147483648))`), `memory size`);

// ./test/core/memory.wast:72
assert_invalid(() => instantiate(`(module (memory 0 4294967295))`), `memory size`);

// ./test/core/memory.wast:103
let $9 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "ABC\\a7D") (data (i32.const 20) "WASM")

  ;; Data section
  (func (export "data") (result i32)
    (i32.and
      (i32.and
        (i32.and
          (i32.eq (i32.load8_u (i32.const 0)) (i32.const 65))
          (i32.eq (i32.load8_u (i32.const 3)) (i32.const 167))
        )
        (i32.and
          (i32.eq (i32.load8_u (i32.const 6)) (i32.const 0))
          (i32.eq (i32.load8_u (i32.const 19)) (i32.const 0))
        )
      )
      (i32.and
        (i32.and
          (i32.eq (i32.load8_u (i32.const 20)) (i32.const 87))
          (i32.eq (i32.load8_u (i32.const 23)) (i32.const 77))
        )
        (i32.and
          (i32.eq (i32.load8_u (i32.const 24)) (i32.const 0))
          (i32.eq (i32.load8_u (i32.const 1023)) (i32.const 0))
        )
      )
    )
  )

  ;; Memory cast
  (func (export "cast") (result f64)
    (i64.store (i32.const 8) (i64.const -12345))
    (if
      (f64.eq
        (f64.load (i32.const 8))
        (f64.reinterpret_i64 (i64.const -12345))
      )
      (then (return (f64.const 0)))
    )
    (i64.store align=1 (i32.const 9) (i64.const 0))
    (i32.store16 align=1 (i32.const 15) (i32.const 16453))
    (f64.load align=1 (i32.const 9))
  )

  ;; Sign and zero extending memory loads
  (func (export "i32_load8_s") (param $$i i32) (result i32)
    (i32.store8 (i32.const 8) (local.get $$i))
    (i32.load8_s (i32.const 8))
  )
  (func (export "i32_load8_u") (param $$i i32) (result i32)
    (i32.store8 (i32.const 8) (local.get $$i))
    (i32.load8_u (i32.const 8))
  )
  (func (export "i32_load16_s") (param $$i i32) (result i32)
    (i32.store16 (i32.const 8) (local.get $$i))
    (i32.load16_s (i32.const 8))
  )
  (func (export "i32_load16_u") (param $$i i32) (result i32)
    (i32.store16 (i32.const 8) (local.get $$i))
    (i32.load16_u (i32.const 8))
  )
  (func (export "i64_load8_s") (param $$i i64) (result i64)
    (i64.store8 (i32.const 8) (local.get $$i))
    (i64.load8_s (i32.const 8))
  )
  (func (export "i64_load8_u") (param $$i i64) (result i64)
    (i64.store8 (i32.const 8) (local.get $$i))
    (i64.load8_u (i32.const 8))
  )
  (func (export "i64_load16_s") (param $$i i64) (result i64)
    (i64.store16 (i32.const 8) (local.get $$i))
    (i64.load16_s (i32.const 8))
  )
  (func (export "i64_load16_u") (param $$i i64) (result i64)
    (i64.store16 (i32.const 8) (local.get $$i))
    (i64.load16_u (i32.const 8))
  )
  (func (export "i64_load32_s") (param $$i i64) (result i64)
    (i64.store32 (i32.const 8) (local.get $$i))
    (i64.load32_s (i32.const 8))
  )
  (func (export "i64_load32_u") (param $$i i64) (result i64)
    (i64.store32 (i32.const 8) (local.get $$i))
    (i64.load32_u (i32.const 8))
  )
)`);

// ./test/core/memory.wast:191
assert_return(() => invoke($9, `data`, []), [value("i32", 1)]);

// ./test/core/memory.wast:192
assert_return(() => invoke($9, `cast`, []), [value("f64", 42)]);

// ./test/core/memory.wast:194
assert_return(() => invoke($9, `i32_load8_s`, [-1]), [value("i32", -1)]);

// ./test/core/memory.wast:195
assert_return(() => invoke($9, `i32_load8_u`, [-1]), [value("i32", 255)]);

// ./test/core/memory.wast:196
assert_return(() => invoke($9, `i32_load16_s`, [-1]), [value("i32", -1)]);

// ./test/core/memory.wast:197
assert_return(() => invoke($9, `i32_load16_u`, [-1]), [value("i32", 65535)]);

// ./test/core/memory.wast:199
assert_return(() => invoke($9, `i32_load8_s`, [100]), [value("i32", 100)]);

// ./test/core/memory.wast:200
assert_return(() => invoke($9, `i32_load8_u`, [200]), [value("i32", 200)]);

// ./test/core/memory.wast:201
assert_return(() => invoke($9, `i32_load16_s`, [20000]), [value("i32", 20000)]);

// ./test/core/memory.wast:202
assert_return(() => invoke($9, `i32_load16_u`, [40000]), [value("i32", 40000)]);

// ./test/core/memory.wast:204
assert_return(() => invoke($9, `i32_load8_s`, [-19110589]), [value("i32", 67)]);

// ./test/core/memory.wast:205
assert_return(() => invoke($9, `i32_load8_s`, [878104047]), [value("i32", -17)]);

// ./test/core/memory.wast:206
assert_return(() => invoke($9, `i32_load8_u`, [-19110589]), [value("i32", 67)]);

// ./test/core/memory.wast:207
assert_return(() => invoke($9, `i32_load8_u`, [878104047]), [value("i32", 239)]);

// ./test/core/memory.wast:208
assert_return(() => invoke($9, `i32_load16_s`, [-19110589]), [value("i32", 25923)]);

// ./test/core/memory.wast:209
assert_return(() => invoke($9, `i32_load16_s`, [878104047]), [value("i32", -12817)]);

// ./test/core/memory.wast:210
assert_return(() => invoke($9, `i32_load16_u`, [-19110589]), [value("i32", 25923)]);

// ./test/core/memory.wast:211
assert_return(() => invoke($9, `i32_load16_u`, [878104047]), [value("i32", 52719)]);

// ./test/core/memory.wast:213
assert_return(() => invoke($9, `i64_load8_s`, [-1n]), [value("i64", -1n)]);

// ./test/core/memory.wast:214
assert_return(() => invoke($9, `i64_load8_u`, [-1n]), [value("i64", 255n)]);

// ./test/core/memory.wast:215
assert_return(() => invoke($9, `i64_load16_s`, [-1n]), [value("i64", -1n)]);

// ./test/core/memory.wast:216
assert_return(() => invoke($9, `i64_load16_u`, [-1n]), [value("i64", 65535n)]);

// ./test/core/memory.wast:217
assert_return(() => invoke($9, `i64_load32_s`, [-1n]), [value("i64", -1n)]);

// ./test/core/memory.wast:218
assert_return(() => invoke($9, `i64_load32_u`, [-1n]), [value("i64", 4294967295n)]);

// ./test/core/memory.wast:220
assert_return(() => invoke($9, `i64_load8_s`, [100n]), [value("i64", 100n)]);

// ./test/core/memory.wast:221
assert_return(() => invoke($9, `i64_load8_u`, [200n]), [value("i64", 200n)]);

// ./test/core/memory.wast:222
assert_return(() => invoke($9, `i64_load16_s`, [20000n]), [value("i64", 20000n)]);

// ./test/core/memory.wast:223
assert_return(() => invoke($9, `i64_load16_u`, [40000n]), [value("i64", 40000n)]);

// ./test/core/memory.wast:224
assert_return(() => invoke($9, `i64_load32_s`, [20000n]), [value("i64", 20000n)]);

// ./test/core/memory.wast:225
assert_return(() => invoke($9, `i64_load32_u`, [40000n]), [value("i64", 40000n)]);

// ./test/core/memory.wast:227
assert_return(() => invoke($9, `i64_load8_s`, [-81985529755441853n]), [value("i64", 67n)]);

// ./test/core/memory.wast:228
assert_return(() => invoke($9, `i64_load8_s`, [3771275841602506223n]), [value("i64", -17n)]);

// ./test/core/memory.wast:229
assert_return(() => invoke($9, `i64_load8_u`, [-81985529755441853n]), [value("i64", 67n)]);

// ./test/core/memory.wast:230
assert_return(() => invoke($9, `i64_load8_u`, [3771275841602506223n]), [value("i64", 239n)]);

// ./test/core/memory.wast:231
assert_return(() => invoke($9, `i64_load16_s`, [-81985529755441853n]), [value("i64", 25923n)]);

// ./test/core/memory.wast:232
assert_return(() => invoke($9, `i64_load16_s`, [3771275841602506223n]), [value("i64", -12817n)]);

// ./test/core/memory.wast:233
assert_return(() => invoke($9, `i64_load16_u`, [-81985529755441853n]), [value("i64", 25923n)]);

// ./test/core/memory.wast:234
assert_return(() => invoke($9, `i64_load16_u`, [3771275841602506223n]), [value("i64", 52719n)]);

// ./test/core/memory.wast:235
assert_return(() => invoke($9, `i64_load32_s`, [-81985529755441853n]), [value("i64", 1446274371n)]);

// ./test/core/memory.wast:236
assert_return(() => invoke($9, `i64_load32_s`, [3771275841602506223n]), [value("i64", -1732588049n)]);

// ./test/core/memory.wast:237
assert_return(() => invoke($9, `i64_load32_u`, [-81985529755441853n]), [value("i64", 1446274371n)]);

// ./test/core/memory.wast:238
assert_return(() => invoke($9, `i64_load32_u`, [3771275841602506223n]), [value("i64", 2562379247n)]);

// ./test/core/memory.wast:242
assert_malformed(
  () => instantiate(`(memory $$foo 1) (memory $$foo 1) `),
  `duplicate memory`,
);

// ./test/core/memory.wast:246
assert_malformed(
  () => instantiate(`(import "" "" (memory $$foo 1)) (memory $$foo 1) `),
  `duplicate memory`,
);

// ./test/core/memory.wast:250
assert_malformed(
  () => instantiate(`(import "" "" (memory $$foo 1)) (import "" "" (memory $$foo 1)) `),
  `duplicate memory`,
);

// ./test/core/memory.wast:257
let $10 = instantiate(`(module
  (memory (export "memory") 1 1)

  ;; These should not change the behavior of memory accesses.
  (global (export "__data_end") i32 (i32.const 10000))
  (global (export "__stack_top") i32 (i32.const 10000))
  (global (export "__heap_base") i32 (i32.const 10000))

  (func (export "load") (param i32) (result i32)
    (i32.load8_u (local.get 0))
  )
)`);

// ./test/core/memory.wast:271
assert_return(() => invoke($10, `load`, [0]), [value("i32", 0)]);

// ./test/core/memory.wast:272
assert_return(() => invoke($10, `load`, [10000]), [value("i32", 0)]);

// ./test/core/memory.wast:273
assert_return(() => invoke($10, `load`, [20000]), [value("i32", 0)]);

// ./test/core/memory.wast:274
assert_return(() => invoke($10, `load`, [30000]), [value("i32", 0)]);

// ./test/core/memory.wast:275
assert_return(() => invoke($10, `load`, [40000]), [value("i32", 0)]);

// ./test/core/memory.wast:276
assert_return(() => invoke($10, `load`, [50000]), [value("i32", 0)]);

// ./test/core/memory.wast:277
assert_return(() => invoke($10, `load`, [60000]), [value("i32", 0)]);

// ./test/core/memory.wast:278
assert_return(() => invoke($10, `load`, [65535]), [value("i32", 0)]);
