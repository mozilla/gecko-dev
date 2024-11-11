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

// ./test/core/elem.wast

// ./test/core/elem.wast:4
let $0 = instantiate(`(module
  (table $$t 10 funcref)
  (func $$f)
  (func $$g)

  ;; Passive
  (elem funcref)
  (elem funcref (ref.func $$f) (item ref.func $$f) (item (ref.null func)) (ref.func $$g))
  (elem func)
  (elem func $$f $$f $$g $$g)

  (elem $$p1 funcref)
  (elem $$p2 funcref (ref.func $$f) (ref.func $$f) (ref.null func) (ref.func $$g))
  (elem $$p3 func)
  (elem $$p4 func $$f $$f $$g $$g)

  ;; Active
  (elem (table $$t) (i32.const 0) funcref)
  (elem (table $$t) (i32.const 0) funcref (ref.func $$f) (ref.null func))
  (elem (table $$t) (i32.const 0) func)
  (elem (table $$t) (i32.const 0) func $$f $$g)
  (elem (table $$t) (offset (i32.const 0)) funcref)
  (elem (table $$t) (offset (i32.const 0)) func $$f $$g)
  (elem (table 0) (i32.const 0) func)
  (elem (table 0x0) (i32.const 0) func $$f $$f)
  (elem (table 0x000) (offset (i32.const 0)) func)
  (elem (table 0) (offset (i32.const 0)) func $$f $$f)
  (elem (table $$t) (i32.const 0) func)
  (elem (table $$t) (i32.const 0) func $$f $$f)
  (elem (table $$t) (offset (i32.const 0)) func)
  (elem (table $$t) (offset (i32.const 0)) func $$f $$f)
  (elem (offset (i32.const 0)))
  (elem (offset (i32.const 0)) funcref (ref.func $$f) (ref.null func))
  (elem (offset (i32.const 0)) func $$f $$f)
  (elem (offset (i32.const 0)) $$f $$f)
  (elem (i32.const 0))
  (elem (i32.const 0) funcref (ref.func $$f) (ref.null func))
  (elem (i32.const 0) func $$f $$f)
  (elem (i32.const 0) $$f $$f)
  (elem (i32.const 0) funcref (item (ref.func $$f)) (item (ref.null func)))

  (elem $$a1 (table $$t) (i32.const 0) funcref)
  (elem $$a2 (table $$t) (i32.const 0) funcref (ref.func $$f) (ref.null func))
  (elem $$a3 (table $$t) (i32.const 0) func)
  (elem $$a4 (table $$t) (i32.const 0) func $$f $$g)
  (elem $$a9 (table $$t) (offset (i32.const 0)) funcref)
  (elem $$a10 (table $$t) (offset (i32.const 0)) func $$f $$g)
  (elem $$a11 (table 0) (i32.const 0) func)
  (elem $$a12 (table 0x0) (i32.const 0) func $$f $$f)
  (elem $$a13 (table 0x000) (offset (i32.const 0)) func)
  (elem $$a14 (table 0) (offset (i32.const 0)) func $$f $$f)
  (elem $$a15 (table $$t) (i32.const 0) func)
  (elem $$a16 (table $$t) (i32.const 0) func $$f $$f)
  (elem $$a17 (table $$t) (offset (i32.const 0)) func)
  (elem $$a18 (table $$t) (offset (i32.const 0)) func $$f $$f)
  (elem $$a19 (offset (i32.const 0)))
  (elem $$a20 (offset (i32.const 0)) funcref (ref.func $$f) (ref.null func))
  (elem $$a21 (offset (i32.const 0)) func $$f $$f)
  (elem $$a22 (offset (i32.const 0)) $$f $$f)
  (elem $$a23 (i32.const 0))
  (elem $$a24 (i32.const 0) funcref (ref.func $$f) (ref.null func))
  (elem $$a25 (i32.const 0) func $$f $$f)
  (elem $$a26 (i32.const 0) $$f $$f)

  ;; Declarative
  (elem declare funcref)
  (elem declare funcref (ref.func $$f) (ref.func $$f) (ref.null func) (ref.func $$g))
  (elem declare func)
  (elem declare func $$f $$f $$g $$g)

  (elem $$d1 declare funcref)
  (elem $$d2 declare funcref (ref.func $$f) (ref.func $$f) (ref.null func) (ref.func $$g))
  (elem $$d3 declare func)
  (elem $$d4 declare func $$f $$f $$g $$g)
)`);

// ./test/core/elem.wast:80
let $1 = instantiate(`(module
  (func $$f)
  (func $$g)

  (table $$t funcref (elem (ref.func $$f) (ref.null func) (ref.func $$g)))
)`);

// ./test/core/elem.wast:87
let $2 = instantiate(`(module
  (func $$f)
  (func $$g)

  (table $$t 10 (ref func) (ref.func $$f))
  (elem (i32.const 3) $$g)
)`);

// ./test/core/elem.wast:98
let $3 = instantiate(`(module
  (table 10 funcref)
  (func $$f)
  (elem (i32.const 0) $$f)
)`);

// ./test/core/elem.wast:103
let $4 = instantiate(`(module
  (import "spectest" "table" (table 10 funcref))
  (func $$f)
  (elem (i32.const 0) $$f)
)`);

// ./test/core/elem.wast:109
let $5 = instantiate(`(module
  (table 10 funcref)
  (func $$f)
  (elem (i32.const 0) $$f)
  (elem (i32.const 3) $$f)
  (elem (i32.const 7) $$f)
  (elem (i32.const 5) $$f)
  (elem (i32.const 3) $$f)
)`);

// ./test/core/elem.wast:118
let $6 = instantiate(`(module
  (import "spectest" "table" (table 10 funcref))
  (func $$f)
  (elem (i32.const 9) $$f)
  (elem (i32.const 3) $$f)
  (elem (i32.const 7) $$f)
  (elem (i32.const 3) $$f)
  (elem (i32.const 5) $$f)
)`);

// ./test/core/elem.wast:128
let $7 = instantiate(`(module
  (global (import "spectest" "global_i32") i32)
  (table 1000 funcref)
  (func $$f)
  (elem (global.get 0) $$f)
)`);

// ./test/core/elem.wast:135
let $8 = instantiate(`(module
  (global $$g (import "spectest" "global_i32") i32)
  (table 1000 funcref)
  (func $$f)
  (elem (global.get $$g) $$f)
)`);

// ./test/core/elem.wast:142
let $9 = instantiate(`(module
  (type $$out-i32 (func (result i32)))
  (table 10 funcref)
  (elem (i32.const 7) $$const-i32-a)
  (elem (i32.const 9) $$const-i32-b)
  (func $$const-i32-a (type $$out-i32) (i32.const 65))
  (func $$const-i32-b (type $$out-i32) (i32.const 66))
  (func (export "call-7") (type $$out-i32)
    (call_indirect (type $$out-i32) (i32.const 7))
  )
  (func (export "call-9") (type $$out-i32)
    (call_indirect (type $$out-i32) (i32.const 9))
  )
)`);

// ./test/core/elem.wast:156
assert_return(() => invoke($9, `call-7`, []), [value("i32", 65)]);

// ./test/core/elem.wast:157
assert_return(() => invoke($9, `call-9`, []), [value("i32", 66)]);

// ./test/core/elem.wast:161
let $10 = instantiate(`(module
  (type $$out-i32 (func (result i32)))
  (table 11 funcref)
  (elem (i32.const 6) funcref (ref.null func) (ref.func $$const-i32-a))
  (elem (i32.const 9) funcref (ref.func $$const-i32-b) (ref.null func))
  (func $$const-i32-a (type $$out-i32) (i32.const 65))
  (func $$const-i32-b (type $$out-i32) (i32.const 66))
  (func (export "call-7") (type $$out-i32)
    (call_indirect (type $$out-i32) (i32.const 7))
  )
  (func (export "call-9") (type $$out-i32)
    (call_indirect (type $$out-i32) (i32.const 9))
  )
)`);

// ./test/core/elem.wast:175
assert_return(() => invoke($10, `call-7`, []), [value("i32", 65)]);

// ./test/core/elem.wast:176
assert_return(() => invoke($10, `call-9`, []), [value("i32", 66)]);

// ./test/core/elem.wast:178
let $11 = instantiate(`(module
  (global i32 (i32.const 0))
  (table 1 funcref) (elem (global.get 0) $$f) (func $$f)
)`);

// ./test/core/elem.wast:182
let $12 = instantiate(`(module
  (global $$g i32 (i32.const 0))
  (table 1 funcref) (elem (global.get $$g) $$f) (func $$f)
)`);

// ./test/core/elem.wast:190
let $13 = instantiate(`(module
  (table 10 funcref)
  (func $$f)
  (elem (i32.const 9) $$f)
)`);

// ./test/core/elem.wast:195
let $14 = instantiate(`(module
  (import "spectest" "table" (table 10 funcref))
  (func $$f)
  (elem (i32.const 9) $$f)
)`);

// ./test/core/elem.wast:201
let $15 = instantiate(`(module
  (table 0 funcref)
  (elem (i32.const 0))
)`);

// ./test/core/elem.wast:205
let $16 = instantiate(`(module
  (import "spectest" "table" (table 0 funcref))
  (elem (i32.const 0))
)`);

// ./test/core/elem.wast:210
let $17 = instantiate(`(module
  (table 0 0 funcref)
  (elem (i32.const 0))
)`);

// ./test/core/elem.wast:215
let $18 = instantiate(`(module
  (table 20 funcref)
  (elem (i32.const 20))
)`);

// ./test/core/elem.wast:220
let $19 = instantiate(`(module
  (import "spectest" "table" (table 0 funcref))
  (func $$f)
  (elem (i32.const 0) $$f)
)`);

// ./test/core/elem.wast:226
let $20 = instantiate(`(module
  (import "spectest" "table" (table 0 100 funcref))
  (func $$f)
  (elem (i32.const 0) $$f)
)`);

// ./test/core/elem.wast:232
let $21 = instantiate(`(module
  (import "spectest" "table" (table 0 funcref))
  (func $$f)
  (elem (i32.const 1) $$f)
)`);

// ./test/core/elem.wast:238
let $22 = instantiate(`(module
  (import "spectest" "table" (table 0 30 funcref))
  (func $$f)
  (elem (i32.const 1) $$f)
)`);

// ./test/core/elem.wast:247
let $23 = instantiate(`(module
  (func)
  (table 1 funcref)
  (elem (i32.const 0) func 0)
)`);

// ./test/core/elem.wast:252
let $24 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\04\\01"                ;; Table section: 1 table
    "\\70\\00\\01"              ;; Table 0: [1..] funcref
  "\\09\\07\\01"                ;; Elem section: 1 element segment
    "\\00\\41\\00\\0b\\01\\00"     ;; Segment 0: (i32.const 0) func 0
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:264
let $25 = instantiate(`(module
  (func)
  (table 1 funcref)
  (elem func 0)
)`);

// ./test/core/elem.wast:269
let $26 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\04\\01"                ;; Table section: 1 table
    "\\70\\00\\01"              ;; Table 0: [1..] funcref
  "\\09\\05\\01"                ;; Elem section: 1 element segment
    "\\01\\00\\01\\00"           ;; Segment 0: func 0
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:281
let $27 = instantiate(`(module
  (func)
  (table 1 funcref)
  (elem (table 0) (i32.const 0) func 0)
)`);

// ./test/core/elem.wast:286
let $28 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\04\\01"                ;; Table section: 1 table
    "\\70\\00\\01"              ;; Table 0: [1..] funcref
  "\\09\\09\\01"                ;; Elem section: 1 element segment
    "\\02\\00\\41\\00\\0b\\00\\01\\00"  ;; Segment 0: (table 0) (i32.const 0) func 0
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:298
let $29 = instantiate(`(module
  (func)
  (table 1 funcref)
  (elem declare func 0)
)`);

// ./test/core/elem.wast:303
let $30 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\04\\01"                ;; Table section: 1 table
    "\\70\\00\\01"              ;; Table 0: [1..] funcref
  "\\09\\05\\01"                ;; Elem section: 1 element segment
    "\\03\\00\\01\\00"           ;; Segment 0: declare func 0
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:315
let $31 = instantiate(`(module
  (func)
  (table 1 funcref)
  (elem (i32.const 0) (;;)(ref func) (ref.func 0))
)`);

// ./test/core/elem.wast:320
let $32 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\04\\01"                ;; Table section: 1 table
    "\\70\\00\\01"              ;; Table 0: [1..] funcref
  "\\09\\09\\01"                ;; Elem section: 1 element segment
    "\\04\\41\\00\\0b\\01\\d2\\00\\0b"  ;; Segment 0: (i32.const 0) (ref.func 0)
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:331
let $33 = instantiate(`(module
  (func)
  (table 1 funcref)
  (elem (i32.const 0) funcref (ref.null func))
)`);

// ./test/core/elem.wast:336
let $34 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\04\\01"                ;; Table section: 1 table
    "\\70\\00\\01"              ;; Table 0: [1..] funcref
  "\\09\\09\\01"                ;; Elem section: 1 element segment
    "\\04\\41\\00\\0b\\01\\d0\\70\\0b"  ;; Segment 0: (i32.const 0) (ref.null func)
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:348
let $35 = instantiate(`(module
  (func)
  (table 1 funcref)
  (elem (i32.const 0) funcref (ref.func 0))
)`);

// ./test/core/elem.wast:353
let $36 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\04\\01"                ;; Table section: 1 table
    "\\70\\00\\01"              ;; Table 0: [1..] funcref
  "\\09\\07\\01"                ;; Elem section: 1 element segment
    "\\05\\70\\01\\d2\\00\\0b"     ;; Segment 0: funcref (ref.func 0)
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:364
let $37 = instantiate(`(module
  (func)
  (table 1 funcref)
  (elem (i32.const 0) funcref (ref.null func))
)`);

// ./test/core/elem.wast:369
let $38 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\04\\01"                ;; Table section: 1 table
    "\\70\\00\\01"              ;; Table 0: [1..] funcref
  "\\09\\07\\01"                ;; Elem section: 1 element segment
    "\\05\\70\\01\\d0\\70\\0b"     ;; Segment 0: funcref (ref.null func)
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:381
let $39 = instantiate(`(module
  (func)
  (table 1 funcref)
  (elem (table 0) (i32.const 0) funcref (ref.func 0))
)`);

// ./test/core/elem.wast:386
let $40 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\04\\01"                ;; Table section: 1 table
    "\\70\\00\\01"              ;; Table 0: [1..] funcref
  "\\09\\0b\\01"                ;; Elem section: 1 element segment
    "\\06\\00\\41\\00\\0b\\70\\01\\d2\\00\\0b"  ;; Segment 0: (table 0) (i32.const 0) funcref (ref.func 0)
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:397
let $41 = instantiate(`(module
  (func)
  (table 1 funcref)
  (elem (table 0) (i32.const 0) funcref (ref.null func))
)`);

// ./test/core/elem.wast:402
let $42 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\04\\01"                ;; Table section: 1 table
    "\\70\\00\\01"              ;; Table 0: [1..] funcref
  "\\09\\0b\\01"                ;; Elem section: 1 element segment
    "\\06\\00\\41\\00\\0b\\70\\01\\d0\\70\\0b"  ;; Segment 0: (table 0) (i32.const 0) funcref (ref.null func)
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:414
let $43 = instantiate(`(module
  (func)
  (table 1 funcref)
  (elem declare funcref (ref.func 0))
)`);

// ./test/core/elem.wast:419
let $44 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\04\\01"                ;; Table section: 1 table
    "\\70\\00\\01"              ;; Table 0: [1..] funcref
  "\\09\\07\\01"                ;; Elem section: 1 element segment
    "\\07\\70\\01\\d2\\00\\0b"     ;; Segment 0: declare funcref (ref.func 0)
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:430
let $45 = instantiate(`(module
  (func)
  (table 1 funcref)
  (elem declare funcref (ref.null func))
)`);

// ./test/core/elem.wast:435
let $46 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\04\\01"                ;; Table section: 1 table
    "\\70\\00\\01"              ;; Table 0: [1..] funcref
  "\\09\\07\\01"                ;; Elem section: 1 element segment
    "\\07\\70\\01\\d0\\70\\0b"     ;; Segment 0: declare funcref (ref.null func)
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:448
let $47 = instantiate(`(module
  (func)
  (table 1 (ref func) (ref.func 0))
  (elem (i32.const 0) func 0)
)`);

// ./test/core/elem.wast:453
let $48 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\0a\\01"                ;; Table section: 1 table
    "\\40\\00\\64\\70\\00\\01\\d2\\00\\0b"  ;; Table 0: [1..] (ref func) (ref.func 0)
  "\\09\\07\\01"                ;; Elem section: 1 element segment
    "\\00\\41\\00\\0b\\01\\00"     ;; Segment 0: (i32.const 0) func 0
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:465
let $49 = instantiate(`(module
  (func)
  (table 1 (ref func) (ref.func 0))
  (elem func 0)
)`);

// ./test/core/elem.wast:470
let $50 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\0a\\01"                ;; Table section: 1 table
    "\\40\\00\\64\\70\\00\\01\\d2\\00\\0b"  ;; Table 0: [1..] (ref func) (ref.func 0)
  "\\09\\05\\01"                ;; Elem section: 1 element segment
    "\\01\\00\\01\\00"           ;; Segment 0: func 0
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:482
let $51 = instantiate(`(module
  (func)
  (table 1 (ref func) (ref.func 0))
  (elem (table 0) (i32.const 0) func 0)
)`);

// ./test/core/elem.wast:487
let $52 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\0a\\01"                ;; Table section: 1 table
    "\\40\\00\\64\\70\\00\\01\\d2\\00\\0b"  ;; Table 0: [1..] (ref func) (ref.func 0)
  "\\09\\09\\01"                ;; Elem section: 1 element segment
    "\\02\\00\\41\\00\\0b\\00\\01\\00"  ;; Segment 0: (table 0) (i32.const 0) func 0
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:499
let $53 = instantiate(`(module
  (func)
  (table 1 (ref func) (ref.func 0))
  (elem declare func 0)
)`);

// ./test/core/elem.wast:504
let $54 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\0a\\01"                ;; Table section: 1 table
    "\\40\\00\\64\\70\\00\\01\\d2\\00\\0b"  ;; Table 0: [1..] (ref func) (ref.func 0)
  "\\09\\05\\01"                ;; Elem section: 1 element segment
    "\\03\\00\\01\\00"           ;; Segment 0: declare func 0
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:516
assert_invalid(
  () => instantiate(`(module
    (func)
    (table 1 (ref func) (ref.func 0))
    (elem (i32.const 0) funcref (ref.func 0))
  )`),
  `type mismatch`,
);

// ./test/core/elem.wast:524
assert_invalid(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"    ;; Magic
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\02\\01\\00"             ;; Function section: 1 function
    "\\04\\0a\\01"                ;; Table section: 1 table
      "\\40\\00\\64\\70\\00\\01\\d2\\00\\0b"  ;; Table 0: [1..] (ref func) (ref.func 0)
    "\\09\\09\\01"                ;; Elem section: 1 element segment
      "\\04\\41\\00\\0b\\01\\d2\\00\\0b"  ;; Segment 0: (i32.const 0) (ref.func 0)
    "\\0a\\04\\01"                ;; Code section: 1 function
      "\\02\\00\\0b"              ;; Function 0: empty
  )`),
  `type mismatch`,
);

// ./test/core/elem.wast:539
let $55 = instantiate(`(module
  (func)
  (table 1 (ref func) (ref.func 0))
  (elem (ref func) (ref.func 0))
)`);

// ./test/core/elem.wast:544
let $56 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\0a\\01"                ;; Table section: 1 table
    "\\40\\00\\64\\70\\00\\01\\d2\\00\\0b"  ;; Table 0: [1..] (ref func) (ref.func 0)
  "\\09\\08\\01"                ;; Elem section: 1 element segment
    "\\05\\64\\70\\01\\d2\\00\\0b"  ;; Segment 0: (ref func) (ref.func 0)
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:556
let $57 = instantiate(`(module
  (func)
  (table 1 (ref func) (ref.func 0))
  (elem (table 0) (i32.const 0) (ref func) (ref.func 0))
)`);

// ./test/core/elem.wast:561
let $58 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\0a\\01"                ;; Table section: 1 table
    "\\40\\00\\64\\70\\00\\01\\d2\\00\\0b"  ;; Table 0: [1..] (ref func) (ref.func 0)
  "\\09\\0c\\01"                ;; Elem section: 1 element segment
    "\\06\\00\\41\\00\\0b\\64\\70\\01\\d2\\00\\0b"  ;; Segment 0: (table 0) (i32.const 0) (ref func) (ref.func 0)
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:573
let $59 = instantiate(`(module
  (func)
  (table 1 (ref func) (ref.func 0))
  (elem declare (ref func) (ref.func 0))
)`);

// ./test/core/elem.wast:578
let $60 = instantiate(`(module binary
  "\\00asm" "\\01\\00\\00\\00"    ;; Magic
  "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
  "\\03\\02\\01\\00"             ;; Function section: 1 function
  "\\04\\0a\\01"                ;; Table section: 1 table
    "\\40\\00\\64\\70\\00\\01\\d2\\00\\0b"  ;; Table 0: [1..] (ref func) (ref.func 0)
  "\\09\\08\\01"                ;; Elem section: 1 element segment
    "\\07\\64\\70\\01\\d2\\00\\0b"  ;; Segment 0: declare (ref func) (ref.func 0)
  "\\0a\\04\\01"                ;; Code section: 1 function
    "\\02\\00\\0b"              ;; Function 0: empty
)`);

// ./test/core/elem.wast:593
assert_trap(
  () => instantiate(`(module
    (table 0 funcref)
    (func $$f)
    (elem (i32.const 0) $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/elem.wast:602
assert_trap(
  () => instantiate(`(module
    (table 0 0 funcref)
    (func $$f)
    (elem (i32.const 0) $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/elem.wast:611
assert_trap(
  () => instantiate(`(module
    (table 0 1 funcref)
    (func $$f)
    (elem (i32.const 0) $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/elem.wast:620
assert_trap(
  () => instantiate(`(module
    (table 0 funcref)
    (elem (i32.const 1))
  )`),
  `out of bounds table access`,
);

// ./test/core/elem.wast:627
assert_trap(
  () => instantiate(`(module
    (table 10 funcref)
    (func $$f)
    (elem (i32.const 10) $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/elem.wast:635
assert_trap(
  () => instantiate(`(module
    (import "spectest" "table" (table 10 funcref))
    (func $$f)
    (elem (i32.const 10) $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/elem.wast:644
assert_trap(
  () => instantiate(`(module
    (table 10 20 funcref)
    (func $$f)
    (elem (i32.const 10) $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/elem.wast:652
assert_trap(
  () => instantiate(`(module
    (import "spectest" "table" (table 10 funcref))
    (func $$f)
    (elem (i32.const 10) $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/elem.wast:661
assert_trap(
  () => instantiate(`(module
    (table 10 funcref)
    (func $$f)
    (elem (i32.const -1) $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/elem.wast:669
assert_trap(
  () => instantiate(`(module
    (import "spectest" "table" (table 10 funcref))
    (func $$f)
    (elem (i32.const -1) $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/elem.wast:678
assert_trap(
  () => instantiate(`(module
    (table 10 funcref)
    (func $$f)
    (elem (i32.const -10) $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/elem.wast:686
assert_trap(
  () => instantiate(`(module
    (import "spectest" "table" (table 10 funcref))
    (func $$f)
    (elem (i32.const -10) $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/elem.wast:698
let $61 = instantiate(`(module
  (table 10 funcref)
  (elem $$e (i32.const 0) func $$f)
  (func $$f)
  (func (export "init")
    (table.init $$e (i32.const 0) (i32.const 0) (i32.const 1))
  )
)`);

// ./test/core/elem.wast:706
assert_trap(() => invoke($61, `init`, []), `out of bounds table access`);

// ./test/core/elem.wast:708
let $62 = instantiate(`(module
  (table 10 funcref)
  (elem $$e declare func $$f)
  (func $$f)
  (func (export "init")
    (table.init $$e (i32.const 0) (i32.const 0) (i32.const 1))
  )
)`);

// ./test/core/elem.wast:716
assert_trap(() => invoke($62, `init`, []), `out of bounds table access`);

// ./test/core/elem.wast:721
assert_invalid(
  () => instantiate(`(module
    (func $$f)
    (elem (i32.const 0) $$f)
  )`),
  `unknown table`,
);

// ./test/core/elem.wast:732
assert_invalid(
  () => instantiate(`(module
    (table 1 funcref)
    (elem (i64.const 0))
  )`),
  `type mismatch`,
);

// ./test/core/elem.wast:740
assert_invalid(
  () => instantiate(`(module
    (table 1 funcref)
    (elem (ref.null func))
  )`),
  `type mismatch`,
);

// ./test/core/elem.wast:748
assert_invalid(
  () => instantiate(`(module 
    (table 1 funcref)
    (elem (offset (;empty instruction sequence;)))
  )`),
  `type mismatch`,
);

// ./test/core/elem.wast:756
assert_invalid(
  () => instantiate(`(module
    (table 1 funcref)
    (elem (offset (i32.const 0) (i32.const 0)))
  )`),
  `type mismatch`,
);

// ./test/core/elem.wast:764
assert_invalid(
  () => instantiate(`(module
    (global (import "test" "global-i32") i32)
    (table 1 funcref)
    (elem (offset (global.get 0) (global.get 0)))
  )`),
  `type mismatch`,
);

// ./test/core/elem.wast:773
assert_invalid(
  () => instantiate(`(module
    (global (import "test" "global-i32") i32)
    (table 1 funcref)
    (elem (offset (global.get 0) (i32.const 0)))
  )`),
  `type mismatch`,
);

// ./test/core/elem.wast:783
assert_invalid(
  () => instantiate(`(module
    (table 1 funcref)
    (elem (i32.ctz (i32.const 0)))
  )`),
  `constant expression required`,
);

// ./test/core/elem.wast:791
assert_invalid(
  () => instantiate(`(module
    (table 1 funcref)
    (elem (nop))
  )`),
  `constant expression required`,
);

// ./test/core/elem.wast:799
assert_invalid(
  () => instantiate(`(module
    (table 1 funcref)
    (elem (offset (nop) (i32.const 0)))
  )`),
  `constant expression required`,
);

// ./test/core/elem.wast:807
assert_invalid(
  () => instantiate(`(module
    (table 1 funcref)
    (elem (offset (i32.const 0) (nop)))
  )`),
  `constant expression required`,
);

// ./test/core/elem.wast:815
assert_invalid(
  () => instantiate(`(module
    (global $$g (import "test" "g") (mut i32))
    (table 1 funcref)
    (elem (global.get $$g))
  )`),
  `constant expression required`,
);

// ./test/core/elem.wast:824
assert_invalid(
  () => instantiate(`(module 
     (table 1 funcref)
     (elem (global.get 0))
   )`),
  `unknown global 0`,
);

// ./test/core/elem.wast:832
assert_invalid(
  () => instantiate(`(module
     (global (import "test" "global-i32") i32)
     (table 1 funcref)
     (elem (global.get 1))
   )`),
  `unknown global 1`,
);

// ./test/core/elem.wast:841
assert_invalid(
  () => instantiate(`(module 
     (global (import "test" "global-mut-i32") (mut i32))
     (table 1 funcref)
     (elem (global.get 0))
   )`),
  `constant expression required`,
);

// ./test/core/elem.wast:853
assert_invalid(
  () => instantiate(`(module
    (table 1 funcref)
    (elem (i32.const 0) funcref (ref.null extern))
  )`),
  `type mismatch`,
);

// ./test/core/elem.wast:861
assert_invalid(
  () => instantiate(`(module
    (table 1 funcref)
    (elem (i32.const 0) funcref (item (ref.null func) (ref.null func)))
  )`),
  `type mismatch`,
);

// ./test/core/elem.wast:869
assert_invalid(
  () => instantiate(`(module
    (table 1 funcref)
    (elem (i32.const 0) funcref (i32.const 0))
  )`),
  `type mismatch`,
);

// ./test/core/elem.wast:877
assert_invalid(
  () => instantiate(`(module
    (table 1 funcref)
    (elem (i32.const 0) funcref (item (i32.const 0)))
  )`),
  `type mismatch`,
);

// ./test/core/elem.wast:885
assert_invalid(
  () => instantiate(`(module
    (table 1 funcref)
    (elem (i32.const 0) funcref (item (call $$f)))
    (func $$f (result funcref) (ref.null func))
  )`),
  `constant expression required`,
);

// ./test/core/elem.wast:897
let $63 = instantiate(`(module
  (type $$out-i32 (func (result i32)))
  (table 10 funcref)
  (elem (i32.const 9) $$const-i32-a)
  (elem (i32.const 9) $$const-i32-b)
  (func $$const-i32-a (type $$out-i32) (i32.const 65))
  (func $$const-i32-b (type $$out-i32) (i32.const 66))
  (func (export "call-overwritten") (type $$out-i32)
    (call_indirect (type $$out-i32) (i32.const 9))
  )
)`);

// ./test/core/elem.wast:908
assert_return(() => invoke($63, `call-overwritten`, []), [value("i32", 66)]);

// ./test/core/elem.wast:910
let $64 = instantiate(`(module
  (type $$out-i32 (func (result i32)))
  (import "spectest" "table" (table 10 funcref))
  (elem (i32.const 9) $$const-i32-a)
  (elem (i32.const 9) $$const-i32-b)
  (func $$const-i32-a (type $$out-i32) (i32.const 65))
  (func $$const-i32-b (type $$out-i32) (i32.const 66))
  (func (export "call-overwritten-element") (type $$out-i32)
    (call_indirect (type $$out-i32) (i32.const 9))
  )
)`);

// ./test/core/elem.wast:921
assert_return(() => invoke($64, `call-overwritten-element`, []), [value("i32", 66)]);

// ./test/core/elem.wast:926
let $65 = instantiate(`(module $$module1
  (type $$out-i32 (func (result i32)))
  (table (export "shared-table") 10 funcref)
  (elem (i32.const 8) $$const-i32-a)
  (elem (i32.const 9) $$const-i32-b)
  (func $$const-i32-a (type $$out-i32) (i32.const 65))
  (func $$const-i32-b (type $$out-i32) (i32.const 66))
  (func (export "call-7") (type $$out-i32)
    (call_indirect (type $$out-i32) (i32.const 7))
  )
  (func (export "call-8") (type $$out-i32)
    (call_indirect (type $$out-i32) (i32.const 8))
  )
  (func (export "call-9") (type $$out-i32)
    (call_indirect (type $$out-i32) (i32.const 9))
  )
)`);
let $module1 = $65;

// ./test/core/elem.wast:944
register($module1, `module1`);

// ./test/core/elem.wast:946
assert_trap(() => invoke($module1, `call-7`, []), `uninitialized element`);

// ./test/core/elem.wast:947
assert_return(() => invoke($module1, `call-8`, []), [value("i32", 65)]);

// ./test/core/elem.wast:948
assert_return(() => invoke($module1, `call-9`, []), [value("i32", 66)]);

// ./test/core/elem.wast:950
let $66 = instantiate(`(module $$module2
  (type $$out-i32 (func (result i32)))
  (import "module1" "shared-table" (table 10 funcref))
  (elem (i32.const 7) $$const-i32-c)
  (elem (i32.const 8) $$const-i32-d)
  (func $$const-i32-c (type $$out-i32) (i32.const 67))
  (func $$const-i32-d (type $$out-i32) (i32.const 68))
)`);
let $module2 = $66;

// ./test/core/elem.wast:959
assert_return(() => invoke($module1, `call-7`, []), [value("i32", 67)]);

// ./test/core/elem.wast:960
assert_return(() => invoke($module1, `call-8`, []), [value("i32", 68)]);

// ./test/core/elem.wast:961
assert_return(() => invoke($module1, `call-9`, []), [value("i32", 66)]);

// ./test/core/elem.wast:963
let $67 = instantiate(`(module $$module3
  (type $$out-i32 (func (result i32)))
  (import "module1" "shared-table" (table 10 funcref))
  (elem (i32.const 8) $$const-i32-e)
  (elem (i32.const 9) $$const-i32-f)
  (func $$const-i32-e (type $$out-i32) (i32.const 69))
  (func $$const-i32-f (type $$out-i32) (i32.const 70))
)`);
let $module3 = $67;

// ./test/core/elem.wast:972
assert_return(() => invoke($module1, `call-7`, []), [value("i32", 67)]);

// ./test/core/elem.wast:973
assert_return(() => invoke($module1, `call-8`, []), [value("i32", 69)]);

// ./test/core/elem.wast:974
assert_return(() => invoke($module1, `call-9`, []), [value("i32", 70)]);

// ./test/core/elem.wast:978
assert_invalid(
  () => instantiate(`(module (func $$f) (table 1 externref) (elem (i32.const 0) $$f))`),
  `type mismatch`,
);

// ./test/core/elem.wast:983
assert_invalid(
  () => instantiate(`(module (table 1 funcref) (elem (i32.const 0) externref (ref.null extern)))`),
  `type mismatch`,
);

// ./test/core/elem.wast:988
assert_invalid(
  () => instantiate(`(module
    (func $$f)
    (table $$t 1 externref)
    (elem $$e funcref (ref.func $$f))
    (func (table.init $$t $$e (i32.const 0) (i32.const 0) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/elem.wast:997
assert_invalid(
  () => instantiate(`(module
    (table $$t 1 funcref)
    (elem $$e externref (ref.null extern))
    (func (table.init $$t $$e (i32.const 0) (i32.const 0) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/elem.wast:1007
let $68 = instantiate(`(module $$m
  (table $$t (export "table") 2 externref)
  (func (export "get") (param $$i i32) (result externref)
        (table.get $$t (local.get $$i)))
  (func (export "set") (param $$i i32) (param $$x externref)
        (table.set $$t (local.get $$i) (local.get $$x))))`);
let $m = $68;

// ./test/core/elem.wast:1014
register($m, `exporter`);

// ./test/core/elem.wast:1016
assert_return(() => invoke($m, `get`, [0]), [value('externref', null)]);

// ./test/core/elem.wast:1017
assert_return(() => invoke($m, `get`, [1]), [value('externref', null)]);

// ./test/core/elem.wast:1019
assert_return(() => invoke($m, `set`, [0, externref(42)]), []);

// ./test/core/elem.wast:1020
assert_return(() => invoke($m, `set`, [1, externref(137)]), []);

// ./test/core/elem.wast:1022
assert_return(() => invoke($m, `get`, [0]), [new ExternRefResult(42)]);

// ./test/core/elem.wast:1023
assert_return(() => invoke($m, `get`, [1]), [new ExternRefResult(137)]);

// ./test/core/elem.wast:1025
let $69 = instantiate(`(module
  (import "exporter" "table" (table $$t 2 externref))
  (elem (i32.const 0) externref (ref.null extern)))`);

// ./test/core/elem.wast:1029
assert_return(() => invoke($m, `get`, [0]), [value('externref', null)]);

// ./test/core/elem.wast:1030
assert_return(() => invoke($m, `get`, [1]), [new ExternRefResult(137)]);

// ./test/core/elem.wast:1034
let $70 = instantiate(`(module $$module4
  (func (result i32)
    i32.const 42
  )
  (global (export "f") funcref (ref.func 0))
)`);
let $module4 = $70;

// ./test/core/elem.wast:1041
register($module4, `module4`);

// ./test/core/elem.wast:1043
let $71 = instantiate(`(module
  (import "module4" "f" (global funcref))
  (type $$out-i32 (func (result i32)))
  (table 10 funcref)
  (elem (offset (i32.const 0)) funcref (global.get 0))
  (func (export "call_imported_elem") (type $$out-i32)
    (call_indirect (type $$out-i32) (i32.const 0))
  )
)`);

// ./test/core/elem.wast:1053
assert_return(() => invoke($71, `call_imported_elem`, []), [value("i32", 42)]);

// ./test/core/elem.wast:1057
let $72 = instantiate(`(module
  (table 10 funcref)
  (func (result i32) (i32.const 42))
  (func (export "call_in_table") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
  (elem (table 0) (offset (i32.add (i32.const 1) (i32.const 2))) funcref (ref.func 0))
)`);

// ./test/core/elem.wast:1065
assert_return(() => invoke($72, `call_in_table`, [3]), [value("i32", 42)]);

// ./test/core/elem.wast:1066
assert_trap(() => invoke($72, `call_in_table`, [0]), `uninitialized element`);

// ./test/core/elem.wast:1068
let $73 = instantiate(`(module
  (table 10 funcref)
  (func (result i32) (i32.const 42))
  (func (export "call_in_table") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
  (elem (table 0) (offset (i32.sub (i32.const 2) (i32.const 1))) funcref (ref.func 0))
)`);

// ./test/core/elem.wast:1076
assert_return(() => invoke($73, `call_in_table`, [1]), [value("i32", 42)]);

// ./test/core/elem.wast:1077
assert_trap(() => invoke($73, `call_in_table`, [0]), `uninitialized element`);

// ./test/core/elem.wast:1079
let $74 = instantiate(`(module
  (table 10 funcref)
  (func (result i32) (i32.const 42))
  (func (export "call_in_table") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
  (elem (table 0) (offset (i32.mul (i32.const 2) (i32.const 2))) funcref (ref.func 0))
)`);

// ./test/core/elem.wast:1087
assert_return(() => invoke($74, `call_in_table`, [4]), [value("i32", 42)]);

// ./test/core/elem.wast:1088
assert_trap(() => invoke($74, `call_in_table`, [0]), `uninitialized element`);

// ./test/core/elem.wast:1092
let $75 = instantiate(`(module
  (global (import "spectest" "global_i32") i32)
  (table 10 funcref)
  (func (result i32) (i32.const 42))
  (func (export "call_in_table") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0)))
  (elem (table 0)
        (offset
          (i32.mul
            (i32.const 2)
            (i32.add
              (i32.sub (global.get 0) (i32.const 665))
              (i32.const 2))))
        funcref
        (ref.func 0))
)`);

// ./test/core/elem.wast:1109
assert_return(() => invoke($75, `call_in_table`, [6]), [value("i32", 42)]);

// ./test/core/elem.wast:1110
assert_trap(() => invoke($75, `call_in_table`, [0]), `uninitialized element`);
