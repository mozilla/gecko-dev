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

// ./test/core/data.wast

// ./test/core/data.wast:5
let $0 = instantiate(`(module
  (memory $$m 1)
  (data (i32.const 0))
  (data (i32.const 1) "a" "" "bcd")
  (data (offset (i32.const 0)))
  (data (offset (i32.const 0)) "" "a" "bc" "")
  (data (memory 0) (i32.const 0))
  (data (memory 0x0) (i32.const 1) "a" "" "bcd")
  (data (memory 0x000) (offset (i32.const 0)))
  (data (memory 0) (offset (i32.const 0)) "" "a" "bc" "")
  (data (memory $$m) (i32.const 0))
  (data (memory $$m) (i32.const 1) "a" "" "bcd")
  (data (memory $$m) (offset (i32.const 0)))
  (data (memory $$m) (offset (i32.const 0)) "" "a" "bc" "")

  (data $$d1 (i32.const 0))
  (data $$d2 (i32.const 1) "a" "" "bcd")
  (data $$d3 (offset (i32.const 0)))
  (data $$d4 (offset (i32.const 0)) "" "a" "bc" "")
  (data $$d5 (memory 0) (i32.const 0))
  (data $$d6 (memory 0x0) (i32.const 1) "a" "" "bcd")
  (data $$d7 (memory 0x000) (offset (i32.const 0)))
  (data $$d8 (memory 0) (offset (i32.const 0)) "" "a" "bc" "")
  (data $$d9 (memory $$m) (i32.const 0))
  (data $$d10 (memory $$m) (i32.const 1) "a" "" "bcd")
  (data $$d11 (memory $$m) (offset (i32.const 0)))
  (data $$d12 (memory $$m) (offset (i32.const 0)) "" "a" "bc" "")
)`);

// ./test/core/data.wast:36
let $1 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "a")
)`);

// ./test/core/data.wast:40
let $2 = instantiate(`(module
  (import "spectest" "memory" (memory 1))
  (data (i32.const 0) "a")
)`);

// ./test/core/data.wast:45
let $3 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "a")
  (data (i32.const 3) "b")
  (data (i32.const 100) "cde")
  (data (i32.const 5) "x")
  (data (i32.const 3) "c")
)`);

// ./test/core/data.wast:53
let $4 = instantiate(`(module
  (import "spectest" "memory" (memory 1))
  (data (i32.const 0) "a")
  (data (i32.const 1) "b")
  (data (i32.const 2) "cde")
  (data (i32.const 3) "f")
  (data (i32.const 2) "g")
  (data (i32.const 1) "h")
)`);

// ./test/core/data.wast:63
let $5 = instantiate(`(module
  (global (import "spectest" "global_i32") i32)
  (memory 1)
  (data (global.get 0) "a")
)`);

// ./test/core/data.wast:68
let $6 = instantiate(`(module
  (global (import "spectest" "global_i32") i32)
  (import "spectest" "memory" (memory 1))
  (data (global.get 0) "a")
)`);

// ./test/core/data.wast:74
let $7 = instantiate(`(module
  (global $$g (import "spectest" "global_i32") i32)
  (memory 1)
  (data (global.get $$g) "a")
)`);

// ./test/core/data.wast:79
let $8 = instantiate(`(module
  (global $$g (import "spectest" "global_i32") i32)
  (import "spectest" "memory" (memory 1))
  (data (global.get $$g) "a")
)`);

// ./test/core/data.wast:85
let $9 = instantiate(`(module (memory 1) (global i32 (i32.const 0)) (data (global.get 0) "a"))`);

// ./test/core/data.wast:86
let $10 = instantiate(`(module (memory 1) (global $$g i32 (i32.const 0)) (data (global.get $$g) "a"))`);

// ./test/core/data.wast:91
let $11 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "a")
  (data (i32.const 0xffff) "b")
)`);

// ./test/core/data.wast:96
let $12 = instantiate(`(module
  (import "spectest" "memory" (memory 1))
  (data (i32.const 0) "a")
  (data (i32.const 0xffff) "b")
)`);

// ./test/core/data.wast:102
let $13 = instantiate(`(module
  (memory 2)
  (data (i32.const 0x1_ffff) "a")
)`);

// ./test/core/data.wast:107
let $14 = instantiate(`(module
  (memory 0)
  (data (i32.const 0))
)`);

// ./test/core/data.wast:111
let $15 = instantiate(`(module
  (import "spectest" "memory" (memory 0))
  (data (i32.const 0))
)`);

// ./test/core/data.wast:116
let $16 = instantiate(`(module
  (memory 0 0)
  (data (i32.const 0))
)`);

// ./test/core/data.wast:121
let $17 = instantiate(`(module
  (memory 1)
  (data (i32.const 0x1_0000) "")
)`);

// ./test/core/data.wast:126
let $18 = instantiate(`(module
  (memory 0)
  (data (i32.const 0) "" "")
)`);

// ./test/core/data.wast:130
let $19 = instantiate(`(module
  (import "spectest" "memory" (memory 0))
  (data (i32.const 0) "" "")
)`);

// ./test/core/data.wast:135
let $20 = instantiate(`(module
  (memory 0 0)
  (data (i32.const 0) "" "")
)`);

// ./test/core/data.wast:140
let $21 = instantiate(`(module
  (import "spectest" "memory" (memory 0))
  (data (i32.const 0) "a")
)`);

// ./test/core/data.wast:145
let $22 = instantiate(`(module
  (import "spectest" "memory" (memory 0 3))
  (data (i32.const 0) "a")
)`);

// ./test/core/data.wast:150
let $23 = instantiate(`(module
  (global (import "spectest" "global_i32") i32)
  (import "spectest" "memory" (memory 0))
  (data (global.get 0) "a")
)`);

// ./test/core/data.wast:156
let $24 = instantiate(`(module
  (global (import "spectest" "global_i32") i32)
  (import "spectest" "memory" (memory 0 3))
  (data (global.get 0) "a")
)`);

// ./test/core/data.wast:162
let $25 = instantiate(`(module
  (import "spectest" "memory" (memory 0))
  (data (i32.const 1) "a")
)`);

// ./test/core/data.wast:167
let $26 = instantiate(`(module
  (import "spectest" "memory" (memory 0 3))
  (data (i32.const 1) "a")
)`);

// ./test/core/data.wast:174
let $27 = instantiate(`(module
  (memory 1)
  (data (i32.add (i32.const 0) (i32.const 42)))
)`);

// ./test/core/data.wast:179
let $28 = instantiate(`(module
  (memory 1)
  (data (i32.sub (i32.const 42) (i32.const 0)))
)`);

// ./test/core/data.wast:184
let $29 = instantiate(`(module
  (memory 1)
  (data (i32.mul (i32.const 1) (i32.const 2)))
)`);

// ./test/core/data.wast:191
let $30 = instantiate(`(module
  (global (import "spectest" "global_i32") i32)
  (memory 1)
  (data (i32.mul
          (i32.const 2)
          (i32.add
            (i32.sub (global.get 0) (i32.const 1))
            (i32.const 2)
          )
        )
  )
)`);

// ./test/core/data.wast:206
assert_trap(
  () => instantiate(`(module
    (memory 0)
    (data (i32.const 0) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:214
assert_trap(
  () => instantiate(`(module
    (memory 0 0)
    (data (i32.const 0) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:222
assert_trap(
  () => instantiate(`(module
    (memory 0 1)
    (data (i32.const 0) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:229
assert_trap(
  () => instantiate(`(module
    (memory 0)
    (data (i32.const 1))
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:236
assert_trap(
  () => instantiate(`(module
    (memory 0 1)
    (data (i32.const 1))
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:253
assert_trap(
  () => instantiate(`(module
    (global (import "spectest" "global_i32") i32)
    (memory 0)
    (data (global.get 0) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:262
assert_trap(
  () => instantiate(`(module
    (memory 1 2)
    (data (i32.const 0x1_0000) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:269
assert_trap(
  () => instantiate(`(module
    (import "spectest" "memory" (memory 1))
    (data (i32.const 0x1_0000) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:277
assert_trap(
  () => instantiate(`(module
    (memory 2)
    (data (i32.const 0x2_0000) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:285
assert_trap(
  () => instantiate(`(module
    (memory 2 3)
    (data (i32.const 0x2_0000) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:293
assert_trap(
  () => instantiate(`(module
    (memory 1)
    (data (i32.const -1) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:300
assert_trap(
  () => instantiate(`(module
    (import "spectest" "memory" (memory 1))
    (data (i32.const -1) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:308
assert_trap(
  () => instantiate(`(module
    (memory 2)
    (data (i32.const -100) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:315
assert_trap(
  () => instantiate(`(module
    (import "spectest" "memory" (memory 1))
    (data (i32.const -100) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/data.wast:325
assert_invalid(
  () => instantiate(`(module
    (data (i32.const 0) "")
  )`),
  `unknown memory`,
);

// ./test/core/data.wast:333
assert_invalid(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                             ;; memory section
    "\\00\\00"                                ;; memory 0
    "\\0b\\07\\01"                             ;; data section
    "\\02\\01\\41\\00\\0b"                       ;; active data segment 0 for memory 1
    "\\00"                                   ;; empty vec(byte)
  )`),
  `unknown memory 1`,
);

// ./test/core/data.wast:346
assert_invalid(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\0b\\06\\01"                             ;; data section
    "\\00\\41\\00\\0b"                          ;; active data segment 0 for memory 0
    "\\00"                                   ;; empty vec(byte)
  )`),
  `unknown memory 0`,
);

// ./test/core/data.wast:357
assert_invalid(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\0b\\07\\01"                             ;; data section
    "\\02\\01\\41\\00\\0b"                       ;; active data segment 0 for memory 1
    "\\00"                                   ;; empty vec(byte)
  )`),
  `unknown memory 1`,
);

// ./test/core/data.wast:369
assert_invalid(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\05\\03\\01"                             ;; memory section
    "\\00\\00"                                ;; memory 0
    "\\0b\\45\\01"                             ;; data section
    "\\02"                                   ;; active segment
    "\\01"                                   ;; memory index
    "\\41\\00\\0b"                             ;; offset constant expression
    "\\3e"                                   ;; vec(byte) length
    "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f"
    "\\10\\11\\12\\13\\14\\15\\16\\17\\18\\19\\1a\\1b\\1c\\1d\\1e\\1f"
    "\\20\\21\\22\\23\\24\\25\\26\\27\\28\\29\\2a\\2b\\2c\\2d\\2e\\2f"
    "\\30\\31\\32\\33\\34\\35\\36\\37\\38\\39\\3a\\3b\\3c\\3d"
  )`),
  `unknown memory 1`,
);

// ./test/core/data.wast:391
assert_invalid(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\0b\\45\\01"                             ;; data section
    "\\02"                                   ;; active segment
    "\\01"                                   ;; memory index
    "\\41\\00\\0b"                             ;; offset constant expression
    "\\3e"                                   ;; vec(byte) length
    "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f"
    "\\10\\11\\12\\13\\14\\15\\16\\17\\18\\19\\1a\\1b\\1c\\1d\\1e\\1f"
    "\\20\\21\\22\\23\\24\\25\\26\\27\\28\\29\\2a\\2b\\2c\\2d\\2e\\2f"
    "\\30\\31\\32\\33\\34\\35\\36\\37\\38\\39\\3a\\3b\\3c\\3d"
  )`),
  `unknown memory 1`,
);

// ./test/core/data.wast:410
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data (i64.const 0))
  )`),
  `type mismatch`,
);

// ./test/core/data.wast:418
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data (ref.null func))
  )`),
  `type mismatch`,
);

// ./test/core/data.wast:426
assert_invalid(
  () => instantiate(`(module 
    (memory 1)
    (data (offset (;empty instruction sequence;)))
  )`),
  `type mismatch`,
);

// ./test/core/data.wast:434
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data (offset (i32.const 0) (i32.const 0)))
  )`),
  `type mismatch`,
);

// ./test/core/data.wast:442
assert_invalid(
  () => instantiate(`(module
    (global (import "test" "global-i32") i32)
    (memory 1)
    (data (offset (global.get 0) (global.get 0)))
  )`),
  `type mismatch`,
);

// ./test/core/data.wast:451
assert_invalid(
  () => instantiate(`(module
    (global (import "test" "global-i32") i32)
    (memory 1)
    (data (offset (global.get 0) (i32.const 0)))
  )`),
  `type mismatch`,
);

// ./test/core/data.wast:460
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data (i32.ctz (i32.const 0)))
  )`),
  `constant expression required`,
);

// ./test/core/data.wast:468
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data (nop))
  )`),
  `constant expression required`,
);

// ./test/core/data.wast:476
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data (offset (nop) (i32.const 0)))
  )`),
  `constant expression required`,
);

// ./test/core/data.wast:484
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data (offset (i32.const 0) (nop)))
  )`),
  `constant expression required`,
);

// ./test/core/data.wast:492
assert_invalid(
  () => instantiate(`(module
    (global $$g (import "test" "g") (mut i32))
    (memory 1)
    (data (global.get $$g))
  )`),
  `constant expression required`,
);

// ./test/core/data.wast:501
assert_invalid(
  () => instantiate(`(module 
     (memory 1)
     (data (global.get 0))
   )`),
  `unknown global 0`,
);

// ./test/core/data.wast:509
assert_invalid(
  () => instantiate(`(module
     (global (import "test" "global-i32") i32)
     (memory 1)
     (data (global.get 1))
   )`),
  `unknown global 1`,
);

// ./test/core/data.wast:518
assert_invalid(
  () => instantiate(`(module 
     (global (import "test" "global-mut-i32") (mut i32))
     (memory 1)
     (data (global.get 0))
   )`),
  `constant expression required`,
);
