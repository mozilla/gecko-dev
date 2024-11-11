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

// ./test/core/linking.wast

// ./test/core/linking.wast:3
let $0 = instantiate(`(module $$Mf
  (func (export "call") (result i32) (call $$g))
  (func $$g (result i32) (i32.const 2))
)`);
let $Mf = $0;

// ./test/core/linking.wast:7
register($Mf, `Mf`);

// ./test/core/linking.wast:9
let $1 = instantiate(`(module $$Nf
  (func $$f (import "Mf" "call") (result i32))
  (export "Mf.call" (func $$f))
  (func (export "call Mf.call") (result i32) (call $$f))
  (func (export "call") (result i32) (call $$g))
  (func $$g (result i32) (i32.const 3))
)`);
let $Nf = $1;

// ./test/core/linking.wast:17
assert_return(() => invoke($Mf, `call`, []), [value("i32", 2)]);

// ./test/core/linking.wast:18
assert_return(() => invoke($Nf, `Mf.call`, []), [value("i32", 2)]);

// ./test/core/linking.wast:19
assert_return(() => invoke($Nf, `call`, []), [value("i32", 3)]);

// ./test/core/linking.wast:20
assert_return(() => invoke($Nf, `call Mf.call`, []), [value("i32", 2)]);

// ./test/core/linking.wast:22
let $2 = instantiate(`(module
  (import "spectest" "print_i32" (func $$f (param i32)))
  (export "print" (func $$f))
)`);

// ./test/core/linking.wast:26
register($2, `reexport_f`);

// ./test/core/linking.wast:27
assert_unlinkable(
  () => instantiate(`(module (import "reexport_f" "print" (func (param i64))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:31
assert_unlinkable(
  () => instantiate(`(module (import "reexport_f" "print" (func (param i32) (result i32))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:39
let $3 = instantiate(`(module $$Mg
  (global $$glob (export "glob") i32 (i32.const 42))
  (func (export "get") (result i32) (global.get $$glob))

  ;; export mutable globals
  (global $$mut_glob (export "mut_glob") (mut i32) (i32.const 142))
  (func (export "get_mut") (result i32) (global.get $$mut_glob))
  (func (export "set_mut") (param i32) (global.set $$mut_glob (local.get 0)))
)`);
let $Mg = $3;

// ./test/core/linking.wast:48
register($Mg, `Mg`);

// ./test/core/linking.wast:50
let $4 = instantiate(`(module $$Ng
  (global $$x (import "Mg" "glob") i32)
  (global $$mut_glob (import "Mg" "mut_glob") (mut i32))
  (func $$f (import "Mg" "get") (result i32))
  (func $$get_mut (import "Mg" "get_mut") (result i32))
  (func $$set_mut (import "Mg" "set_mut") (param i32))

  (export "Mg.glob" (global $$x))
  (export "Mg.get" (func $$f))
  (global $$glob (export "glob") i32 (i32.const 43))
  (func (export "get") (result i32) (global.get $$glob))

  (export "Mg.mut_glob" (global $$mut_glob))
  (export "Mg.get_mut" (func $$get_mut))
  (export "Mg.set_mut" (func $$set_mut))
)`);
let $Ng = $4;

// ./test/core/linking.wast:67
assert_return(() => get($Mg, `glob`), [value("i32", 42)]);

// ./test/core/linking.wast:68
assert_return(() => get($Ng, `Mg.glob`), [value("i32", 42)]);

// ./test/core/linking.wast:69
assert_return(() => get($Ng, `glob`), [value("i32", 43)]);

// ./test/core/linking.wast:70
assert_return(() => invoke($Mg, `get`, []), [value("i32", 42)]);

// ./test/core/linking.wast:71
assert_return(() => invoke($Ng, `Mg.get`, []), [value("i32", 42)]);

// ./test/core/linking.wast:72
assert_return(() => invoke($Ng, `get`, []), [value("i32", 43)]);

// ./test/core/linking.wast:74
assert_return(() => get($Mg, `mut_glob`), [value("i32", 142)]);

// ./test/core/linking.wast:75
assert_return(() => get($Ng, `Mg.mut_glob`), [value("i32", 142)]);

// ./test/core/linking.wast:76
assert_return(() => invoke($Mg, `get_mut`, []), [value("i32", 142)]);

// ./test/core/linking.wast:77
assert_return(() => invoke($Ng, `Mg.get_mut`, []), [value("i32", 142)]);

// ./test/core/linking.wast:79
assert_return(() => invoke($Mg, `set_mut`, [241]), []);

// ./test/core/linking.wast:80
assert_return(() => get($Mg, `mut_glob`), [value("i32", 241)]);

// ./test/core/linking.wast:81
assert_return(() => get($Ng, `Mg.mut_glob`), [value("i32", 241)]);

// ./test/core/linking.wast:82
assert_return(() => invoke($Mg, `get_mut`, []), [value("i32", 241)]);

// ./test/core/linking.wast:83
assert_return(() => invoke($Ng, `Mg.get_mut`, []), [value("i32", 241)]);

// ./test/core/linking.wast:86
assert_unlinkable(
  () => instantiate(`(module (import "Mg" "mut_glob" (global i32)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:90
assert_unlinkable(
  () => instantiate(`(module (import "Mg" "glob" (global (mut i32))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:96
let $5 = instantiate(`(module $$Mref_ex
  (type $$t (func))
  (func $$f) (elem declare func $$f)
  (global (export "g-const-funcnull") (ref null func) (ref.null func))
  (global (export "g-const-func") (ref func) (ref.func $$f))
  (global (export "g-const-refnull") (ref null $$t) (ref.null $$t))
  (global (export "g-const-ref") (ref $$t) (ref.func $$f))
  (global (export "g-const-extern") externref (ref.null extern))
  (global (export "g-var-funcnull") (mut (ref null func)) (ref.null func))
  (global (export "g-var-func") (mut (ref func)) (ref.func $$f))
  (global (export "g-var-refnull") (mut (ref null $$t)) (ref.null $$t))
  (global (export "g-var-ref") (mut (ref $$t)) (ref.func $$f))
  (global (export "g-var-extern") (mut externref) (ref.null extern))
)`);
let $Mref_ex = $5;

// ./test/core/linking.wast:110
register($Mref_ex, `Mref_ex`);

// ./test/core/linking.wast:112
let $6 = instantiate(`(module $$Mref_im
  (type $$t (func))
  (global (import "Mref_ex" "g-const-funcnull") (ref null func))
  (global (import "Mref_ex" "g-const-func") (ref null func))
  (global (import "Mref_ex" "g-const-refnull") (ref null func))
  (global (import "Mref_ex" "g-const-ref") (ref null func))
  (global (import "Mref_ex" "g-const-func") (ref func))
  (global (import "Mref_ex" "g-const-ref") (ref func))
  (global (import "Mref_ex" "g-const-refnull") (ref null $$t))
  (global (import "Mref_ex" "g-const-ref") (ref null $$t))
  (global (import "Mref_ex" "g-const-ref") (ref $$t))
  (global (import "Mref_ex" "g-const-extern") externref)

  (global (import "Mref_ex" "g-var-funcnull") (mut (ref null func)))
  (global (import "Mref_ex" "g-var-func") (mut (ref func)))
  (global (import "Mref_ex" "g-var-refnull") (mut (ref null $$t)))
  (global (import "Mref_ex" "g-var-ref") (mut (ref $$t)))
  (global (import "Mref_ex" "g-var-extern") (mut externref))
)`);
let $Mref_im = $6;

// ./test/core/linking.wast:132
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-const-extern") (ref null func)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:137
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-const-funcnull") (ref func)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:141
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-const-refnull") (ref func)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:145
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-const-extern") (ref func)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:150
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-const-funcnull") (ref null $$t)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:154
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-const-func") (ref null $$t)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:158
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-const-extern") (ref null $$t)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:163
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-const-funcnull") (ref $$t)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:167
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-const-func") (ref $$t)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:171
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-const-refnull") (ref $$t)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:175
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-const-extern") (ref $$t)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:181
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-func") (mut (ref null func))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:185
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-refnull") (mut (ref null func))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:189
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-ref") (mut (ref null func))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:193
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-extern") (mut (ref null func))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:198
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-funcnull") (mut (ref func))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:202
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-refnull") (mut (ref func))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:206
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-ref") (mut (ref func))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:210
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-extern") (mut (ref func))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:215
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-var-funcnull") (mut (ref null $$t))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:219
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-var-func") (mut (ref null $$t))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:223
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-var-ref") (mut (ref null $$t))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:227
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-var-extern") (mut (ref null $$t))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:232
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-var-funcnull") (mut (ref $$t))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:236
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-var-func") (mut (ref $$t))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:240
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-var-refnull") (mut (ref $$t))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:244
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (global (import "Mref_ex" "g-var-extern") (mut (ref $$t))))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:249
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-funcnull") (mut externref)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:253
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-func") (mut externref)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:257
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-refnull") (mut externref)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:261
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-ref") (mut externref)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:269
let $7 = instantiate(`(module $$Mt
  (type (func (result i32)))
  (type (func))

  (table (export "tab") 10 funcref)
  (elem (i32.const 2) $$g $$g $$g $$g)
  (func $$g (result i32) (i32.const 4))
  (func (export "h") (result i32) (i32.const -4))

  (func (export "call") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0))
  )
)`);
let $Mt = $7;

// ./test/core/linking.wast:282
register($Mt, `Mt`);

// ./test/core/linking.wast:284
let $8 = instantiate(`(module $$Nt
  (type (func))
  (type (func (result i32)))

  (func $$f (import "Mt" "call") (param i32) (result i32))
  (func $$h (import "Mt" "h") (result i32))

  (table funcref (elem $$g $$g $$g $$h $$f))
  (func $$g (result i32) (i32.const 5))

  (export "Mt.call" (func $$f))
  (func (export "call Mt.call") (param i32) (result i32)
    (call $$f (local.get 0))
  )
  (func (export "call") (param i32) (result i32)
    (call_indirect (type 1) (local.get 0))
  )
)`);
let $Nt = $8;

// ./test/core/linking.wast:303
assert_return(() => invoke($Mt, `call`, [2]), [value("i32", 4)]);

// ./test/core/linking.wast:304
assert_return(() => invoke($Nt, `Mt.call`, [2]), [value("i32", 4)]);

// ./test/core/linking.wast:305
assert_return(() => invoke($Nt, `call`, [2]), [value("i32", 5)]);

// ./test/core/linking.wast:306
assert_return(() => invoke($Nt, `call Mt.call`, [2]), [value("i32", 4)]);

// ./test/core/linking.wast:308
assert_trap(() => invoke($Mt, `call`, [1]), `uninitialized element`);

// ./test/core/linking.wast:309
assert_trap(() => invoke($Nt, `Mt.call`, [1]), `uninitialized element`);

// ./test/core/linking.wast:310
assert_return(() => invoke($Nt, `call`, [1]), [value("i32", 5)]);

// ./test/core/linking.wast:311
assert_trap(() => invoke($Nt, `call Mt.call`, [1]), `uninitialized element`);

// ./test/core/linking.wast:313
assert_trap(() => invoke($Mt, `call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:314
assert_trap(() => invoke($Nt, `Mt.call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:315
assert_return(() => invoke($Nt, `call`, [0]), [value("i32", 5)]);

// ./test/core/linking.wast:316
assert_trap(() => invoke($Nt, `call Mt.call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:318
assert_trap(() => invoke($Mt, `call`, [20]), `undefined element`);

// ./test/core/linking.wast:319
assert_trap(() => invoke($Nt, `Mt.call`, [20]), `undefined element`);

// ./test/core/linking.wast:320
assert_trap(() => invoke($Nt, `call`, [7]), `undefined element`);

// ./test/core/linking.wast:321
assert_trap(() => invoke($Nt, `call Mt.call`, [20]), `undefined element`);

// ./test/core/linking.wast:323
assert_return(() => invoke($Nt, `call`, [3]), [value("i32", -4)]);

// ./test/core/linking.wast:324
assert_trap(() => invoke($Nt, `call`, [4]), `indirect call type mismatch`);

// ./test/core/linking.wast:326
let $9 = instantiate(`(module $$Ot
  (type (func (result i32)))

  (func $$h (import "Mt" "h") (result i32))
  (table (import "Mt" "tab") 5 funcref)
  (elem (i32.const 1) $$i $$h)
  (func $$i (result i32) (i32.const 6))

  (func (export "call") (param i32) (result i32)
    (call_indirect (type 0) (local.get 0))
  )
)`);
let $Ot = $9;

// ./test/core/linking.wast:339
assert_return(() => invoke($Mt, `call`, [3]), [value("i32", 4)]);

// ./test/core/linking.wast:340
assert_return(() => invoke($Nt, `Mt.call`, [3]), [value("i32", 4)]);

// ./test/core/linking.wast:341
assert_return(() => invoke($Nt, `call Mt.call`, [3]), [value("i32", 4)]);

// ./test/core/linking.wast:342
assert_return(() => invoke($Ot, `call`, [3]), [value("i32", 4)]);

// ./test/core/linking.wast:344
assert_return(() => invoke($Mt, `call`, [2]), [value("i32", -4)]);

// ./test/core/linking.wast:345
assert_return(() => invoke($Nt, `Mt.call`, [2]), [value("i32", -4)]);

// ./test/core/linking.wast:346
assert_return(() => invoke($Nt, `call`, [2]), [value("i32", 5)]);

// ./test/core/linking.wast:347
assert_return(() => invoke($Nt, `call Mt.call`, [2]), [value("i32", -4)]);

// ./test/core/linking.wast:348
assert_return(() => invoke($Ot, `call`, [2]), [value("i32", -4)]);

// ./test/core/linking.wast:350
assert_return(() => invoke($Mt, `call`, [1]), [value("i32", 6)]);

// ./test/core/linking.wast:351
assert_return(() => invoke($Nt, `Mt.call`, [1]), [value("i32", 6)]);

// ./test/core/linking.wast:352
assert_return(() => invoke($Nt, `call`, [1]), [value("i32", 5)]);

// ./test/core/linking.wast:353
assert_return(() => invoke($Nt, `call Mt.call`, [1]), [value("i32", 6)]);

// ./test/core/linking.wast:354
assert_return(() => invoke($Ot, `call`, [1]), [value("i32", 6)]);

// ./test/core/linking.wast:356
assert_trap(() => invoke($Mt, `call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:357
assert_trap(() => invoke($Nt, `Mt.call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:358
assert_return(() => invoke($Nt, `call`, [0]), [value("i32", 5)]);

// ./test/core/linking.wast:359
assert_trap(() => invoke($Nt, `call Mt.call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:360
assert_trap(() => invoke($Ot, `call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:362
assert_trap(() => invoke($Ot, `call`, [20]), `undefined element`);

// ./test/core/linking.wast:364
let $10 = instantiate(`(module
  (table (import "Mt" "tab") 0 funcref)
  (elem (i32.const 9) $$f)
  (func $$f)
)`);

// ./test/core/linking.wast:370
let $11 = instantiate(`(module $$G1 (global (export "g") i32 (i32.const 5)))`);
let $G1 = $11;

// ./test/core/linking.wast:371
register($G1, `G1`);

// ./test/core/linking.wast:372
let $12 = instantiate(`(module $$G2
  (global (import "G1" "g") i32)
  (global (export "g") i32 (global.get 0))
)`);
let $G2 = $12;

// ./test/core/linking.wast:376
assert_return(() => get($G2, `g`), [value("i32", 5)]);

// ./test/core/linking.wast:378
assert_trap(
  () => instantiate(`(module
    (table (import "Mt" "tab") 0 funcref)
    (elem (i32.const 10) $$f)
    (func $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/linking.wast:387
assert_unlinkable(
  () => instantiate(`(module
    (table (import "Mt" "tab") 10 funcref)
    (memory (import "Mt" "mem") 1)  ;; does not exist
    (func $$f (result i32) (i32.const 0))
    (elem (i32.const 7) $$f)
    (elem (i32.const 9) $$f)
  )`),
  `unknown import`,
);

// ./test/core/linking.wast:397
assert_trap(() => invoke($Mt, `call`, [7]), `uninitialized element`);

// ./test/core/linking.wast:401
assert_trap(
  () => instantiate(`(module
    (table (import "Mt" "tab") 10 funcref)
    (func $$f (result i32) (i32.const 0))
    (elem (i32.const 7) $$f)
    (elem (i32.const 8) $$f $$f $$f $$f $$f)  ;; (partially) out of bounds
  )`),
  `out of bounds table access`,
);

// ./test/core/linking.wast:410
assert_return(() => invoke($Mt, `call`, [7]), [value("i32", 0)]);

// ./test/core/linking.wast:411
assert_trap(() => invoke($Mt, `call`, [8]), `uninitialized element`);

// ./test/core/linking.wast:413
assert_trap(
  () => instantiate(`(module
    (table (import "Mt" "tab") 10 funcref)
    (func $$f (result i32) (i32.const 0))
    (elem (i32.const 7) $$f)
    (memory 1)
    (data (i32.const 0x10000) "d")  ;; out of bounds
  )`),
  `out of bounds memory access`,
);

// ./test/core/linking.wast:423
assert_return(() => invoke($Mt, `call`, [7]), [value("i32", 0)]);

// ./test/core/linking.wast:426
let $13 = instantiate(`(module $$Mtable_ex
  (type $$t (func))
  (table (export "t-funcnull") 1 (ref null func))
  (table (export "t-refnull") 1 (ref null $$t))
  (table (export "t-extern") 1 externref)
)`);
let $Mtable_ex = $13;

// ./test/core/linking.wast:432
register($Mtable_ex, `Mtable_ex`);

// ./test/core/linking.wast:434
let $14 = instantiate(`(module
  (type $$t (func))
  (table (import "Mtable_ex" "t-funcnull") 1 (ref null func))
  (table (import "Mtable_ex" "t-refnull") 1 (ref null $$t))
  (table (import "Mtable_ex" "t-extern") 1 externref)
)`);

// ./test/core/linking.wast:441
assert_unlinkable(
  () => instantiate(`(module (table (import "Mtable_ex" "t-refnull") 1 (ref null func)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:445
assert_unlinkable(
  () => instantiate(`(module (table (import "Mtable_ex" "t-extern") 1 (ref null func)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:450
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (table (import "Mtable_ex" "t-funcnull") 1 (ref null $$t)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:454
assert_unlinkable(
  () => instantiate(`(module (type $$t (func)) (table (import "Mtable_ex" "t-extern") 1 (ref null $$t)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:459
assert_unlinkable(
  () => instantiate(`(module (table (import "Mtable_ex" "t-funcnull") 1 externref))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:463
assert_unlinkable(
  () => instantiate(`(module (table (import "Mtable_ex" "t-refnull") 1 externref))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:471
let $15 = instantiate(`(module $$Mm
  (memory (export "mem") 1 5)
  (data (i32.const 10) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")

  (func (export "load") (param $$a i32) (result i32)
    (i32.load8_u (local.get 0))
  )
)`);
let $Mm = $15;

// ./test/core/linking.wast:479
register($Mm, `Mm`);

// ./test/core/linking.wast:481
let $16 = instantiate(`(module $$Nm
  (func $$loadM (import "Mm" "load") (param i32) (result i32))

  (memory 1)
  (data (i32.const 10) "\\f0\\f1\\f2\\f3\\f4\\f5")

  (export "Mm.load" (func $$loadM))
  (func (export "load") (param $$a i32) (result i32)
    (i32.load8_u (local.get 0))
  )
)`);
let $Nm = $16;

// ./test/core/linking.wast:493
assert_return(() => invoke($Mm, `load`, [12]), [value("i32", 2)]);

// ./test/core/linking.wast:494
assert_return(() => invoke($Nm, `Mm.load`, [12]), [value("i32", 2)]);

// ./test/core/linking.wast:495
assert_return(() => invoke($Nm, `load`, [12]), [value("i32", 242)]);

// ./test/core/linking.wast:497
let $17 = instantiate(`(module $$Om
  (memory (import "Mm" "mem") 1)
  (data (i32.const 5) "\\a0\\a1\\a2\\a3\\a4\\a5\\a6\\a7")

  (func (export "load") (param $$a i32) (result i32)
    (i32.load8_u (local.get 0))
  )
)`);
let $Om = $17;

// ./test/core/linking.wast:506
assert_return(() => invoke($Mm, `load`, [12]), [value("i32", 167)]);

// ./test/core/linking.wast:507
assert_return(() => invoke($Nm, `Mm.load`, [12]), [value("i32", 167)]);

// ./test/core/linking.wast:508
assert_return(() => invoke($Nm, `load`, [12]), [value("i32", 242)]);

// ./test/core/linking.wast:509
assert_return(() => invoke($Om, `load`, [12]), [value("i32", 167)]);

// ./test/core/linking.wast:511
let $18 = instantiate(`(module
  (memory (import "Mm" "mem") 0)
  (data (i32.const 0xffff) "a")
)`);

// ./test/core/linking.wast:516
assert_trap(
  () => instantiate(`(module
    (memory (import "Mm" "mem") 0)
    (data (i32.const 0x10000) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/linking.wast:524
let $19 = instantiate(`(module $$Pm
  (memory (import "Mm" "mem") 1 8)

  (func (export "grow") (param $$a i32) (result i32)
    (memory.grow (local.get 0))
  )
)`);
let $Pm = $19;

// ./test/core/linking.wast:532
assert_return(() => invoke($Pm, `grow`, [0]), [value("i32", 1)]);

// ./test/core/linking.wast:533
assert_return(() => invoke($Pm, `grow`, [2]), [value("i32", 1)]);

// ./test/core/linking.wast:534
assert_return(() => invoke($Pm, `grow`, [0]), [value("i32", 3)]);

// ./test/core/linking.wast:535
assert_return(() => invoke($Pm, `grow`, [1]), [value("i32", 3)]);

// ./test/core/linking.wast:536
assert_return(() => invoke($Pm, `grow`, [1]), [value("i32", 4)]);

// ./test/core/linking.wast:537
assert_return(() => invoke($Pm, `grow`, [0]), [value("i32", 5)]);

// ./test/core/linking.wast:538
assert_return(() => invoke($Pm, `grow`, [1]), [value("i32", -1)]);

// ./test/core/linking.wast:539
assert_return(() => invoke($Pm, `grow`, [0]), [value("i32", 5)]);

// ./test/core/linking.wast:541
assert_unlinkable(
  () => instantiate(`(module
    (func $$host (import "spectest" "print"))
    (memory (import "Mm" "mem") 1)
    (table (import "Mm" "tab") 0 funcref)  ;; does not exist
    (data (i32.const 0) "abc")
  )`),
  `unknown import`,
);

// ./test/core/linking.wast:550
assert_return(() => invoke($Mm, `load`, [0]), [value("i32", 0)]);

// ./test/core/linking.wast:554
assert_trap(
  () => instantiate(`(module
    ;; Note: the memory is 5 pages large by the time we get here.
    (memory (import "Mm" "mem") 1)
    (data (i32.const 0) "abc")
    (data (i32.const 327670) "zzzzzzzzzzzzzzzzzz") ;; (partially) out of bounds
  )`),
  `out of bounds memory access`,
);

// ./test/core/linking.wast:563
assert_return(() => invoke($Mm, `load`, [0]), [value("i32", 97)]);

// ./test/core/linking.wast:564
assert_return(() => invoke($Mm, `load`, [327670]), [value("i32", 0)]);

// ./test/core/linking.wast:566
assert_trap(
  () => instantiate(`(module
    (memory (import "Mm" "mem") 1)
    (data (i32.const 0) "abc")
    (table 0 funcref)
    (func)
    (elem (i32.const 0) 0)  ;; out of bounds
  )`),
  `out of bounds table access`,
);

// ./test/core/linking.wast:576
assert_return(() => invoke($Mm, `load`, [0]), [value("i32", 97)]);

// ./test/core/linking.wast:579
let $20 = instantiate(`(module $$Ms
  (type $$t (func (result i32)))
  (memory (export "memory") 1)
  (table (export "table") 1 funcref)
  (func (export "get memory[0]") (type $$t)
    (i32.load8_u (i32.const 0))
  )
  (func (export "get table[0]") (type $$t)
    (call_indirect (type $$t) (i32.const 0))
  )
)`);
let $Ms = $20;

// ./test/core/linking.wast:590
register($Ms, `Ms`);

// ./test/core/linking.wast:592
assert_trap(
  () => instantiate(`(module
    (import "Ms" "memory" (memory 1))
    (import "Ms" "table" (table 1 funcref))
    (data (i32.const 0) "hello")
    (elem (i32.const 0) $$f)
    (func $$f (result i32)
      (i32.const 0xdead)
    )
    (func $$main
      (unreachable)
    )
    (start $$main)
  )`),
  `unreachable`,
);

// ./test/core/linking.wast:609
assert_return(() => invoke($Ms, `get memory[0]`, []), [value("i32", 104)]);

// ./test/core/linking.wast:610
assert_return(() => invoke($Ms, `get table[0]`, []), [value("i32", 57005)]);
