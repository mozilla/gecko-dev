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

// ./test/core/gc/array_new_data.wast

// ./test/core/gc/array_new_data.wast:1
let $0 = instantiate(`(module
  (type $$arr (array (mut i8)))

  (data $$d "abcd")

  (func (export "array-new-data") (param i32 i32) (result (ref $$arr))
    (array.new_data $$arr $$d (local.get 0) (local.get 1))
  )
)`);

// ./test/core/gc/array_new_data.wast:12
assert_return(() => invoke($0, `array-new-data`, [0, 0]), [new RefWithType('arrayref')]);

// ./test/core/gc/array_new_data.wast:13
assert_return(() => invoke($0, `array-new-data`, [0, 4]), [new RefWithType('arrayref')]);

// ./test/core/gc/array_new_data.wast:14
assert_return(() => invoke($0, `array-new-data`, [1, 2]), [new RefWithType('arrayref')]);

// ./test/core/gc/array_new_data.wast:15
assert_return(() => invoke($0, `array-new-data`, [4, 0]), [new RefWithType('arrayref')]);

// ./test/core/gc/array_new_data.wast:18
assert_trap(() => invoke($0, `array-new-data`, [0, 5]), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:19
assert_trap(() => invoke($0, `array-new-data`, [5, 0]), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:20
assert_trap(() => invoke($0, `array-new-data`, [1, 4]), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:21
assert_trap(() => invoke($0, `array-new-data`, [4, 1]), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:23
let $1 = instantiate(`(module
  (type $$a32 (array i32))
  (type $$a64 (array i64))

  (data $$data0 "")
  (data $$data1 "1")
  (data $$data2 "12")
  (data $$data3 "123")
  (data $$data4 "1234")
  (data $$data7 "1234567")
  (data $$data9 "123456789")

  (func (export "f0")
    (drop (array.new_data $$a32 $$data0 (i32.const 0) (i32.const 1)))
  )
  (func (export "f1")
    (drop (array.new_data $$a32 $$data1 (i32.const 0) (i32.const 1)))
  )
  (func (export "f2")
    (drop (array.new_data $$a32 $$data2 (i32.const 0) (i32.const 1)))
  )
  (func (export "f3")
    (drop (array.new_data $$a32 $$data3 (i32.const 0) (i32.const 1)))
  )
  (func (export "f4")
    (drop (array.new_data $$a32 $$data4 (i32.const 0) (i32.const 1)))
  )
  (func (export "f9")
    (drop (array.new_data $$a32 $$data9 (i32.const 6) (i32.const 1)))
  )

  (func (export "g0")
    (drop (array.new_data $$a64 $$data0 (i32.const 0) (i32.const 1)))
  )
  (func (export "g1")
    (drop (array.new_data $$a64 $$data1 (i32.const 0) (i32.const 1)))
  )
  (func (export "g4")
    (drop (array.new_data $$a64 $$data4 (i32.const 0) (i32.const 1)))
  )
  (func (export "g7")
    (drop (array.new_data $$a64 $$data7 (i32.const 0) (i32.const 1)))
  )
  (func (export "g8")
    (drop (array.new_data $$a64 $$data9 (i32.const 0) (i32.const 1)))
  )
  (func (export "g9")
    (drop (array.new_data $$a64 $$data9 (i32.const 2) (i32.const 1)))
  )
)`);

// ./test/core/gc/array_new_data.wast:74
assert_trap(() => invoke($1, `f0`, []), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:75
assert_trap(() => invoke($1, `f1`, []), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:76
assert_trap(() => invoke($1, `f2`, []), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:77
assert_trap(() => invoke($1, `f3`, []), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:78
assert_return(() => invoke($1, `f4`, []), []);

// ./test/core/gc/array_new_data.wast:79
assert_trap(() => invoke($1, `f9`, []), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:81
assert_trap(() => invoke($1, `g0`, []), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:82
assert_trap(() => invoke($1, `g1`, []), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:83
assert_trap(() => invoke($1, `g4`, []), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:84
assert_trap(() => invoke($1, `g7`, []), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:85
assert_return(() => invoke($1, `g8`, []), []);

// ./test/core/gc/array_new_data.wast:86
assert_trap(() => invoke($1, `g9`, []), `out of bounds memory access`);

// ./test/core/gc/array_new_data.wast:89
let $2 = instantiate(`(module
  (type $$arr (array (mut i8)))

  (data $$d "\\aa\\bb\\cc\\dd")

  (func (export "array-new-data-contents") (result i32 i32)
    (local (ref $$arr))
    (local.set 0 (array.new_data $$arr $$d (i32.const 1) (i32.const 2)))
    (array.get_u $$arr (local.get 0) (i32.const 0))
    (array.get_u $$arr (local.get 0) (i32.const 1))
  )
)`);

// ./test/core/gc/array_new_data.wast:103
assert_return(() => invoke($2, `array-new-data-contents`, []), [value("i32", 187), value("i32", 204)]);

// ./test/core/gc/array_new_data.wast:105
let $3 = instantiate(`(module
  (type $$arr (array (mut i32)))

  (data $$d "\\aa\\bb\\cc\\dd")

  (func (export "array-new-data-little-endian") (result i32)
    (array.get $$arr
               (array.new_data $$arr $$d (i32.const 0) (i32.const 1))
               (i32.const 0))
  )
)`);

// ./test/core/gc/array_new_data.wast:118
assert_return(() => invoke($3, `array-new-data-little-endian`, []), [value("i32", -573785174)]);

// ./test/core/gc/array_new_data.wast:120
let $4 = instantiate(`(module
  (type $$arr (array (mut i16)))

  (data $$d "\\00\\11\\22")

  (func (export "array-new-data-unaligned") (result i32)
    (array.get_u $$arr
                 (array.new_data $$arr $$d (i32.const 1) (i32.const 1))
                 (i32.const 0))
  )
)`);

// ./test/core/gc/array_new_data.wast:133
assert_return(() => invoke($4, `array-new-data-unaligned`, []), [value("i32", 8721)]);
