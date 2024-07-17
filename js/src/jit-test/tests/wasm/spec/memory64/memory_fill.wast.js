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

// ./test/core/memory_fill.wast

// ./test/core/memory_fill.wast:6
let $0 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0xFF00) (i32.const 0x55) (i32.const 256))))`);

// ./test/core/memory_fill.wast:22
invoke($0, `test`, []);

// ./test/core/memory_fill.wast:24
assert_return(() => invoke($0, `checkRange`, [0, 65280, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:26
assert_return(() => invoke($0, `checkRange`, [65280, 65536, 85]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:28
let $1 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0xFF00) (i32.const 0x55) (i32.const 257))))`);

// ./test/core/memory_fill.wast:44
assert_trap(() => invoke($1, `test`, []), `out of bounds memory access`);

// ./test/core/memory_fill.wast:46
let $2 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0xFFFFFF00) (i32.const 0x55) (i32.const 257))))`);

// ./test/core/memory_fill.wast:62
assert_trap(() => invoke($2, `test`, []), `out of bounds memory access`);

// ./test/core/memory_fill.wast:64
let $3 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0x12) (i32.const 0x55) (i32.const 0))))`);

// ./test/core/memory_fill.wast:80
invoke($3, `test`, []);

// ./test/core/memory_fill.wast:82
assert_return(() => invoke($3, `checkRange`, [0, 65536, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:84
let $4 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0x10000) (i32.const 0x55) (i32.const 0))))`);

// ./test/core/memory_fill.wast:100
invoke($4, `test`, []);

// ./test/core/memory_fill.wast:102
let $5 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0x20000) (i32.const 0x55) (i32.const 0))))`);

// ./test/core/memory_fill.wast:118
assert_trap(() => invoke($5, `test`, []), `out of bounds memory access`);

// ./test/core/memory_fill.wast:120
let $6 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i32.const 0x1) (i32.const 0xAA) (i32.const 0xFFFE))))`);

// ./test/core/memory_fill.wast:136
invoke($6, `test`, []);

// ./test/core/memory_fill.wast:138
assert_return(() => invoke($6, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:140
assert_return(() => invoke($6, `checkRange`, [1, 65535, 170]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:142
assert_return(() => invoke($6, `checkRange`, [65535, 65536, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:145
let $7 = instantiate(`(module
  (memory 1 1)
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
     (memory.fill (i32.const 0x12) (i32.const 0x55) (i32.const 10))
     (memory.fill (i32.const 0x15) (i32.const 0xAA) (i32.const 4))))`);

// ./test/core/memory_fill.wast:162
invoke($7, `test`, []);

// ./test/core/memory_fill.wast:164
assert_return(() => invoke($7, `checkRange`, [0, 18, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:166
assert_return(() => invoke($7, `checkRange`, [18, 21, 85]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:168
assert_return(() => invoke($7, `checkRange`, [21, 25, 170]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:170
assert_return(() => invoke($7, `checkRange`, [25, 28, 85]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:172
assert_return(() => invoke($7, `checkRange`, [28, 65536, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:174
assert_invalid(
  () => instantiate(`(module
    (func (export "testfn")
      (memory.fill (i32.const 10) (i32.const 20) (i32.const 30))))`),
  `unknown memory 0`,
);

// ./test/core/memory_fill.wast:180
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:187
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:194
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:201
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:208
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:215
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:222
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:229
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:236
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:243
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:250
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:257
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:264
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:271
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:278
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:285
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:292
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:299
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:306
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:313
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:320
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:327
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:334
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:341
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:348
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:355
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:362
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:369
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:376
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:383
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:390
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:397
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:404
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:411
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:418
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:425
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:432
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:439
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:446
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:453
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:460
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:467
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:474
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:481
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:488
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:495
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:502
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:509
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:516
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:523
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:530
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:537
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:544
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:551
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:558
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:565
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:572
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:579
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:586
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:593
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:600
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:607
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:614
assert_invalid(
  () => instantiate(`(module
    (memory 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:621
let $8 = instantiate(`(module
  (memory 1 1 )
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$val i32) (param $$len i32)
    (memory.fill (local.get $$offs) (local.get $$val) (local.get $$len))))`);

// ./test/core/memory_fill.wast:638
assert_trap(() => invoke($8, `run`, [65280, 37, 512]), `out of bounds memory access`);

// ./test/core/memory_fill.wast:641
assert_return(() => invoke($8, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:643
let $9 = instantiate(`(module
  (memory 1 1 )
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$val i32) (param $$len i32)
    (memory.fill (local.get $$offs) (local.get $$val) (local.get $$len))))`);

// ./test/core/memory_fill.wast:660
assert_trap(() => invoke($9, `run`, [65279, 37, 514]), `out of bounds memory access`);

// ./test/core/memory_fill.wast:663
assert_return(() => invoke($9, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:665
let $10 = instantiate(`(module
  (memory 1 1 )
  
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$val i32) (param $$len i32)
    (memory.fill (local.get $$offs) (local.get $$val) (local.get $$len))))`);

// ./test/core/memory_fill.wast:682
assert_trap(() => invoke($10, `run`, [65279, 37, -1]), `out of bounds memory access`);

// ./test/core/memory_fill.wast:685
assert_return(() => invoke($10, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_fill.wast:688
let $11 = instantiate(`(module
  (memory i64 1 1)
  
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i64.const 0xFF00) (i32.const 0x55) (i64.const 256))))`);

// ./test/core/memory_fill.wast:704
invoke($11, `test`, []);

// ./test/core/memory_fill.wast:706
assert_return(() => invoke($11, `checkRange`, [0n, 65280n, 0]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:708
assert_return(() => invoke($11, `checkRange`, [65280n, 65536n, 85]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:710
let $12 = instantiate(`(module
  (memory i64 1 1)
  
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i64.const 0xFF00) (i32.const 0x55) (i64.const 257))))`);

// ./test/core/memory_fill.wast:726
assert_trap(() => invoke($12, `test`, []), `out of bounds memory access`);

// ./test/core/memory_fill.wast:728
let $13 = instantiate(`(module
  (memory i64 1 1)
  
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i64.const 0xFFFFFF00) (i32.const 0x55) (i64.const 257))))`);

// ./test/core/memory_fill.wast:744
assert_trap(() => invoke($13, `test`, []), `out of bounds memory access`);

// ./test/core/memory_fill.wast:746
let $14 = instantiate(`(module
  (memory i64 1 1)
  
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i64.const 0x12) (i32.const 0x55) (i64.const 0))))`);

// ./test/core/memory_fill.wast:762
invoke($14, `test`, []);

// ./test/core/memory_fill.wast:764
assert_return(() => invoke($14, `checkRange`, [0n, 65536n, 0]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:766
let $15 = instantiate(`(module
  (memory i64 1 1)
  
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i64.const 0x10000) (i32.const 0x55) (i64.const 0))))`);

// ./test/core/memory_fill.wast:782
invoke($15, `test`, []);

// ./test/core/memory_fill.wast:784
let $16 = instantiate(`(module
  (memory i64 1 1)
  
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i64.const 0x20000) (i32.const 0x55) (i64.const 0))))`);

// ./test/core/memory_fill.wast:800
assert_trap(() => invoke($16, `test`, []), `out of bounds memory access`);

// ./test/core/memory_fill.wast:802
let $17 = instantiate(`(module
  (memory i64 1 1)
  
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
    (memory.fill (i64.const 0x1) (i32.const 0xAA) (i64.const 0xFFFE))))`);

// ./test/core/memory_fill.wast:818
invoke($17, `test`, []);

// ./test/core/memory_fill.wast:820
assert_return(() => invoke($17, `checkRange`, [0n, 1n, 0]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:822
assert_return(() => invoke($17, `checkRange`, [1n, 65535n, 170]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:824
assert_return(() => invoke($17, `checkRange`, [65535n, 65536n, 0]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:827
let $18 = instantiate(`(module
  (memory i64 1 1)
  
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "test")
     (memory.fill (i64.const 0x12) (i32.const 0x55) (i64.const 10))
     (memory.fill (i64.const 0x15) (i32.const 0xAA) (i64.const 4))))`);

// ./test/core/memory_fill.wast:844
invoke($18, `test`, []);

// ./test/core/memory_fill.wast:846
assert_return(() => invoke($18, `checkRange`, [0n, 18n, 0]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:848
assert_return(() => invoke($18, `checkRange`, [18n, 21n, 85]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:850
assert_return(() => invoke($18, `checkRange`, [21n, 25n, 170]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:852
assert_return(() => invoke($18, `checkRange`, [25n, 28n, 85]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:854
assert_return(() => invoke($18, `checkRange`, [28n, 65536n, 0]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:856
assert_invalid(
  () => instantiate(`(module
    (func (export "testfn")
      (memory.fill (i64.const 10) (i32.const 20) (i64.const 30))))`),
  `unknown memory 0`,
);

// ./test/core/memory_fill.wast:862
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:869
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:876
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:883
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:890
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:897
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:904
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:911
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:918
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:925
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:932
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:939
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (i64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:946
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:953
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:960
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:967
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i32.const 10) (f64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:974
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:981
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:988
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:995
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1002
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1009
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1016
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1023
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1030
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1037
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1044
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1051
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (i64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1058
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1065
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1072
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1079
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f32.const 10) (f64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1086
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1093
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1100
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1107
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1114
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1121
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1128
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1135
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1142
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1149
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1156
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (i64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1163
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1170
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1177
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1184
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (i64.const 10) (f64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1191
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1198
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1205
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1212
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1219
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f32.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1226
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f32.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1233
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f32.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1240
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f32.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1247
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1254
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1261
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1268
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (i64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1275
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f64.const 20) (i32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1282
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f64.const 20) (f32.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1289
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f64.const 20) (i64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1296
assert_invalid(
  () => instantiate(`(module
    (memory i64 1 1)
    (func (export "testfn")
      (memory.fill (f64.const 10) (f64.const 20) (f64.const 30))))`),
  `type mismatch`,
);

// ./test/core/memory_fill.wast:1303
let $19 = instantiate(`(module
  (memory i64 1 1 )
  
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i64) (param $$val i32) (param $$len i64)
    (memory.fill (local.get $$offs) (local.get $$val) (local.get $$len))))`);

// ./test/core/memory_fill.wast:1320
assert_trap(() => invoke($19, `run`, [65280n, 37, 512n]), `out of bounds memory access`);

// ./test/core/memory_fill.wast:1323
assert_return(() => invoke($19, `checkRange`, [0n, 1n, 0]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:1325
let $20 = instantiate(`(module
  (memory i64 1 1 )
  
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i64) (param $$val i32) (param $$len i64)
    (memory.fill (local.get $$offs) (local.get $$val) (local.get $$len))))`);

// ./test/core/memory_fill.wast:1342
assert_trap(() => invoke($20, `run`, [65279n, 37, 514n]), `out of bounds memory access`);

// ./test/core/memory_fill.wast:1345
assert_return(() => invoke($20, `checkRange`, [0n, 1n, 0]), [value("i64", -1n)]);

// ./test/core/memory_fill.wast:1347
let $21 = instantiate(`(module
  (memory i64 1 1 )
  
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i64) (param $$val i32) (param $$len i64)
    (memory.fill (local.get $$offs) (local.get $$val) (local.get $$len))))`);

// ./test/core/memory_fill.wast:1364
assert_trap(() => invoke($21, `run`, [65279n, 37, 4294967295n]), `out of bounds memory access`);

// ./test/core/memory_fill.wast:1367
assert_return(() => invoke($21, `checkRange`, [0n, 1n, 0]), [value("i64", -1n)]);
