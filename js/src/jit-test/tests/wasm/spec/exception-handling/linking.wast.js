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
  (global (export "g-const-func") funcref (ref.null func))
  (global (export "g-var-func") (mut funcref) (ref.null func))
  (global (export "g-const-extern") externref (ref.null extern))
  (global (export "g-var-extern") (mut externref) (ref.null extern))
)`);
let $Mref_ex = $5;

// ./test/core/linking.wast:102
register($Mref_ex, `Mref_ex`);

// ./test/core/linking.wast:104
let $6 = instantiate(`(module $$Mref_im
  (global (import "Mref_ex" "g-const-func") funcref)
  (global (import "Mref_ex" "g-const-extern") externref)

  (global (import "Mref_ex" "g-var-func") (mut funcref))
  (global (import "Mref_ex" "g-var-extern") (mut externref))
)`);
let $Mref_im = $6;

// ./test/core/linking.wast:112
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-const-extern") funcref))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:116
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-const-func") externref))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:122
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-func") (mut externref)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:126
assert_unlinkable(
  () => instantiate(`(module (global (import "Mref_ex" "g-var-extern") (mut funcref)))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:134
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

// ./test/core/linking.wast:147
register($Mt, `Mt`);

// ./test/core/linking.wast:149
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

// ./test/core/linking.wast:168
assert_return(() => invoke($Mt, `call`, [2]), [value("i32", 4)]);

// ./test/core/linking.wast:169
assert_return(() => invoke($Nt, `Mt.call`, [2]), [value("i32", 4)]);

// ./test/core/linking.wast:170
assert_return(() => invoke($Nt, `call`, [2]), [value("i32", 5)]);

// ./test/core/linking.wast:171
assert_return(() => invoke($Nt, `call Mt.call`, [2]), [value("i32", 4)]);

// ./test/core/linking.wast:173
assert_trap(() => invoke($Mt, `call`, [1]), `uninitialized element`);

// ./test/core/linking.wast:174
assert_trap(() => invoke($Nt, `Mt.call`, [1]), `uninitialized element`);

// ./test/core/linking.wast:175
assert_return(() => invoke($Nt, `call`, [1]), [value("i32", 5)]);

// ./test/core/linking.wast:176
assert_trap(() => invoke($Nt, `call Mt.call`, [1]), `uninitialized element`);

// ./test/core/linking.wast:178
assert_trap(() => invoke($Mt, `call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:179
assert_trap(() => invoke($Nt, `Mt.call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:180
assert_return(() => invoke($Nt, `call`, [0]), [value("i32", 5)]);

// ./test/core/linking.wast:181
assert_trap(() => invoke($Nt, `call Mt.call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:183
assert_trap(() => invoke($Mt, `call`, [20]), `undefined element`);

// ./test/core/linking.wast:184
assert_trap(() => invoke($Nt, `Mt.call`, [20]), `undefined element`);

// ./test/core/linking.wast:185
assert_trap(() => invoke($Nt, `call`, [7]), `undefined element`);

// ./test/core/linking.wast:186
assert_trap(() => invoke($Nt, `call Mt.call`, [20]), `undefined element`);

// ./test/core/linking.wast:188
assert_return(() => invoke($Nt, `call`, [3]), [value("i32", -4)]);

// ./test/core/linking.wast:189
assert_trap(() => invoke($Nt, `call`, [4]), `indirect call type mismatch`);

// ./test/core/linking.wast:191
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

// ./test/core/linking.wast:204
assert_return(() => invoke($Mt, `call`, [3]), [value("i32", 4)]);

// ./test/core/linking.wast:205
assert_return(() => invoke($Nt, `Mt.call`, [3]), [value("i32", 4)]);

// ./test/core/linking.wast:206
assert_return(() => invoke($Nt, `call Mt.call`, [3]), [value("i32", 4)]);

// ./test/core/linking.wast:207
assert_return(() => invoke($Ot, `call`, [3]), [value("i32", 4)]);

// ./test/core/linking.wast:209
assert_return(() => invoke($Mt, `call`, [2]), [value("i32", -4)]);

// ./test/core/linking.wast:210
assert_return(() => invoke($Nt, `Mt.call`, [2]), [value("i32", -4)]);

// ./test/core/linking.wast:211
assert_return(() => invoke($Nt, `call`, [2]), [value("i32", 5)]);

// ./test/core/linking.wast:212
assert_return(() => invoke($Nt, `call Mt.call`, [2]), [value("i32", -4)]);

// ./test/core/linking.wast:213
assert_return(() => invoke($Ot, `call`, [2]), [value("i32", -4)]);

// ./test/core/linking.wast:215
assert_return(() => invoke($Mt, `call`, [1]), [value("i32", 6)]);

// ./test/core/linking.wast:216
assert_return(() => invoke($Nt, `Mt.call`, [1]), [value("i32", 6)]);

// ./test/core/linking.wast:217
assert_return(() => invoke($Nt, `call`, [1]), [value("i32", 5)]);

// ./test/core/linking.wast:218
assert_return(() => invoke($Nt, `call Mt.call`, [1]), [value("i32", 6)]);

// ./test/core/linking.wast:219
assert_return(() => invoke($Ot, `call`, [1]), [value("i32", 6)]);

// ./test/core/linking.wast:221
assert_trap(() => invoke($Mt, `call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:222
assert_trap(() => invoke($Nt, `Mt.call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:223
assert_return(() => invoke($Nt, `call`, [0]), [value("i32", 5)]);

// ./test/core/linking.wast:224
assert_trap(() => invoke($Nt, `call Mt.call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:225
assert_trap(() => invoke($Ot, `call`, [0]), `uninitialized element`);

// ./test/core/linking.wast:227
assert_trap(() => invoke($Ot, `call`, [20]), `undefined element`);

// ./test/core/linking.wast:229
let $10 = instantiate(`(module
  (table (import "Mt" "tab") 0 funcref)
  (elem (i32.const 9) $$f)
  (func $$f)
)`);

// ./test/core/linking.wast:235
let $11 = instantiate(`(module $$G1 (global (export "g") i32 (i32.const 5)))`);
let $G1 = $11;

// ./test/core/linking.wast:236
register($G1, `G1`);

// ./test/core/linking.wast:237
let $12 = instantiate(`(module $$G2
  (global (import "G1" "g") i32)
  (global (export "g") i32 (global.get 0))
)`);
let $G2 = $12;

// ./test/core/linking.wast:241
assert_return(() => get($G2, `g`), [value("i32", 5)]);

// ./test/core/linking.wast:243
assert_trap(
  () => instantiate(`(module
    (table (import "Mt" "tab") 0 funcref)
    (elem (i32.const 10) $$f)
    (func $$f)
  )`),
  `out of bounds table access`,
);

// ./test/core/linking.wast:252
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

// ./test/core/linking.wast:262
assert_trap(() => invoke($Mt, `call`, [7]), `uninitialized element`);

// ./test/core/linking.wast:266
assert_trap(
  () => instantiate(`(module
    (table (import "Mt" "tab") 10 funcref)
    (func $$f (result i32) (i32.const 0))
    (elem (i32.const 7) $$f)
    (elem (i32.const 8) $$f $$f $$f $$f $$f)  ;; (partially) out of bounds
  )`),
  `out of bounds table access`,
);

// ./test/core/linking.wast:275
assert_return(() => invoke($Mt, `call`, [7]), [value("i32", 0)]);

// ./test/core/linking.wast:276
assert_trap(() => invoke($Mt, `call`, [8]), `uninitialized element`);

// ./test/core/linking.wast:278
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

// ./test/core/linking.wast:288
assert_return(() => invoke($Mt, `call`, [7]), [value("i32", 0)]);

// ./test/core/linking.wast:291
let $13 = instantiate(`(module $$Mtable_ex
  (table $$t1 (export "t-func") 1 funcref)
  (table $$t2 (export "t-extern") 1 externref)
)`);
let $Mtable_ex = $13;

// ./test/core/linking.wast:295
register($Mtable_ex, `Mtable_ex`);

// ./test/core/linking.wast:297
let $14 = instantiate(`(module
  (table (import "Mtable_ex" "t-func") 1 funcref)
  (table (import "Mtable_ex" "t-extern") 1 externref)
)`);

// ./test/core/linking.wast:302
assert_unlinkable(
  () => instantiate(`(module (table (import "Mtable_ex" "t-func") 1 externref))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:306
assert_unlinkable(
  () => instantiate(`(module (table (import "Mtable_ex" "t-extern") 1 funcref))`),
  `incompatible import type`,
);

// ./test/core/linking.wast:314
let $15 = instantiate(`(module $$Mm
  (memory (export "mem") 1 5)
  (data (i32.const 10) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09")

  (func (export "load") (param $$a i32) (result i32)
    (i32.load8_u (local.get 0))
  )
)`);
let $Mm = $15;

// ./test/core/linking.wast:322
register($Mm, `Mm`);

// ./test/core/linking.wast:324
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

// ./test/core/linking.wast:336
assert_return(() => invoke($Mm, `load`, [12]), [value("i32", 2)]);

// ./test/core/linking.wast:337
assert_return(() => invoke($Nm, `Mm.load`, [12]), [value("i32", 2)]);

// ./test/core/linking.wast:338
assert_return(() => invoke($Nm, `load`, [12]), [value("i32", 242)]);

// ./test/core/linking.wast:340
let $17 = instantiate(`(module $$Om
  (memory (import "Mm" "mem") 1)
  (data (i32.const 5) "\\a0\\a1\\a2\\a3\\a4\\a5\\a6\\a7")

  (func (export "load") (param $$a i32) (result i32)
    (i32.load8_u (local.get 0))
  )
)`);
let $Om = $17;

// ./test/core/linking.wast:349
assert_return(() => invoke($Mm, `load`, [12]), [value("i32", 167)]);

// ./test/core/linking.wast:350
assert_return(() => invoke($Nm, `Mm.load`, [12]), [value("i32", 167)]);

// ./test/core/linking.wast:351
assert_return(() => invoke($Nm, `load`, [12]), [value("i32", 242)]);

// ./test/core/linking.wast:352
assert_return(() => invoke($Om, `load`, [12]), [value("i32", 167)]);

// ./test/core/linking.wast:354
let $18 = instantiate(`(module
  (memory (import "Mm" "mem") 0)
  (data (i32.const 0xffff) "a")
)`);

// ./test/core/linking.wast:359
assert_trap(
  () => instantiate(`(module
    (memory (import "Mm" "mem") 0)
    (data (i32.const 0x10000) "a")
  )`),
  `out of bounds memory access`,
);

// ./test/core/linking.wast:367
let $19 = instantiate(`(module $$Pm
  (memory (import "Mm" "mem") 1 8)

  (func (export "grow") (param $$a i32) (result i32)
    (memory.grow (local.get 0))
  )
)`);
let $Pm = $19;

// ./test/core/linking.wast:375
assert_return(() => invoke($Pm, `grow`, [0]), [value("i32", 1)]);

// ./test/core/linking.wast:376
assert_return(() => invoke($Pm, `grow`, [2]), [value("i32", 1)]);

// ./test/core/linking.wast:377
assert_return(() => invoke($Pm, `grow`, [0]), [value("i32", 3)]);

// ./test/core/linking.wast:378
assert_return(() => invoke($Pm, `grow`, [1]), [value("i32", 3)]);

// ./test/core/linking.wast:379
assert_return(() => invoke($Pm, `grow`, [1]), [value("i32", 4)]);

// ./test/core/linking.wast:380
assert_return(() => invoke($Pm, `grow`, [0]), [value("i32", 5)]);

// ./test/core/linking.wast:381
assert_return(() => invoke($Pm, `grow`, [1]), [value("i32", -1)]);

// ./test/core/linking.wast:382
assert_return(() => invoke($Pm, `grow`, [0]), [value("i32", 5)]);

// ./test/core/linking.wast:384
assert_unlinkable(
  () => instantiate(`(module
    (func $$host (import "spectest" "print"))
    (memory (import "Mm" "mem") 1)
    (table (import "Mm" "tab") 0 funcref)  ;; does not exist
    (data (i32.const 0) "abc")
  )`),
  `unknown import`,
);

// ./test/core/linking.wast:393
assert_return(() => invoke($Mm, `load`, [0]), [value("i32", 0)]);

// ./test/core/linking.wast:397
assert_trap(
  () => instantiate(`(module
    ;; Note: the memory is 5 pages large by the time we get here.
    (memory (import "Mm" "mem") 1)
    (data (i32.const 0) "abc")
    (data (i32.const 327670) "zzzzzzzzzzzzzzzzzz") ;; (partially) out of bounds
  )`),
  `out of bounds memory access`,
);

// ./test/core/linking.wast:406
assert_return(() => invoke($Mm, `load`, [0]), [value("i32", 97)]);

// ./test/core/linking.wast:407
assert_return(() => invoke($Mm, `load`, [327670]), [value("i32", 0)]);

// ./test/core/linking.wast:409
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

// ./test/core/linking.wast:419
assert_return(() => invoke($Mm, `load`, [0]), [value("i32", 97)]);

// ./test/core/linking.wast:422
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

// ./test/core/linking.wast:433
register($Ms, `Ms`);

// ./test/core/linking.wast:435
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

// ./test/core/linking.wast:452
assert_return(() => invoke($Ms, `get memory[0]`, []), [value("i32", 104)]);

// ./test/core/linking.wast:453
assert_return(() => invoke($Ms, `get table[0]`, []), [value("i32", 57005)]);
