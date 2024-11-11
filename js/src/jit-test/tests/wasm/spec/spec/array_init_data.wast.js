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

// ./test/core/gc/array_init_data.wast

// ./test/core/gc/array_init_data.wast:5
assert_invalid(
  () => instantiate(`(module
    (type $$a (array i8))

    (data $$d1 "a")

    (func (export "array.init_data-immutable") (param $$1 (ref $$a))
      (array.init_data $$a $$d1 (local.get $$1) (i32.const 0) (i32.const 0) (i32.const 0))
    )
  )`),
  `array is immutable`,
);

// ./test/core/gc/array_init_data.wast:18
assert_invalid(
  () => instantiate(`(module
    (type $$a (array (mut funcref)))

    (data $$d1 "a")

    (func (export "array.init_data-invalid-1") (param $$1 (ref $$a))
      (array.init_data $$a $$d1 (local.get $$1) (i32.const 0) (i32.const 0) (i32.const 0))
    )
  )`),
  `array type is not numeric or vector`,
);

// ./test/core/gc/array_init_data.wast:31
let $0 = instantiate(`(module
  (type $$arr8 (array i8))
  (type $$arr8_mut (array (mut i8)))
  (type $$arr16_mut (array (mut i16)))

  (global $$g_arr8 (ref $$arr8) (array.new $$arr8 (i32.const 10) (i32.const 12)))
  (global $$g_arr8_mut (mut (ref $$arr8_mut)) (array.new_default $$arr8_mut (i32.const 12)))
  (global $$g_arr16_mut (ref $$arr16_mut) (array.new_default $$arr16_mut (i32.const 6)))

  (data $$d1 "abcdefghijkl")

  (func (export "array_get_nth") (param $$1 i32) (result i32)
    (array.get_u $$arr8_mut (global.get $$g_arr8_mut) (local.get $$1))
  )

  (func (export "array_get_nth_i16") (param $$1 i32) (result i32)
    (array.get_u $$arr16_mut (global.get $$g_arr16_mut) (local.get $$1))
  )

  (func (export "array_init_data-null")
    (array.init_data $$arr8_mut $$d1 (ref.null $$arr8_mut) (i32.const 0) (i32.const 0) (i32.const 0))
  )

  (func (export "array_init_data") (param $$1 i32) (param $$2 i32) (param $$3 i32)
    (array.init_data $$arr8_mut $$d1 (global.get $$g_arr8_mut) (local.get $$1) (local.get $$2) (local.get $$3))
  )

  (func (export "array_init_data_i16") (param $$1 i32) (param $$2 i32) (param $$3 i32)
    (array.init_data $$arr16_mut $$d1 (global.get $$g_arr16_mut) (local.get $$1) (local.get $$2) (local.get $$3))
  )

  (func (export "drop_segs")
    (data.drop $$d1)
  )
)`);

// ./test/core/gc/array_init_data.wast:68
assert_trap(() => invoke($0, `array_init_data-null`, []), `null array reference`);

// ./test/core/gc/array_init_data.wast:71
assert_trap(() => invoke($0, `array_init_data`, [13, 0, 0]), `out of bounds array access`);

// ./test/core/gc/array_init_data.wast:72
assert_trap(() => invoke($0, `array_init_data`, [0, 13, 0]), `out of bounds memory access`);

// ./test/core/gc/array_init_data.wast:75
assert_trap(() => invoke($0, `array_init_data`, [0, 0, 13]), `out of bounds array access`);

// ./test/core/gc/array_init_data.wast:76
assert_trap(() => invoke($0, `array_init_data`, [0, 0, 13]), `out of bounds array access`);

// ./test/core/gc/array_init_data.wast:77
assert_trap(() => invoke($0, `array_init_data_i16`, [0, 0, 7]), `out of bounds array access`);

// ./test/core/gc/array_init_data.wast:80
assert_return(() => invoke($0, `array_init_data`, [12, 0, 0]), []);

// ./test/core/gc/array_init_data.wast:81
assert_return(() => invoke($0, `array_init_data`, [0, 12, 0]), []);

// ./test/core/gc/array_init_data.wast:82
assert_return(() => invoke($0, `array_init_data_i16`, [0, 6, 0]), []);

// ./test/core/gc/array_init_data.wast:85
assert_return(() => invoke($0, `array_get_nth`, [0]), [value("i32", 0)]);

// ./test/core/gc/array_init_data.wast:86
assert_return(() => invoke($0, `array_get_nth`, [5]), [value("i32", 0)]);

// ./test/core/gc/array_init_data.wast:87
assert_return(() => invoke($0, `array_get_nth`, [11]), [value("i32", 0)]);

// ./test/core/gc/array_init_data.wast:88
assert_trap(() => invoke($0, `array_get_nth`, [12]), `out of bounds array access`);

// ./test/core/gc/array_init_data.wast:89
assert_return(() => invoke($0, `array_get_nth_i16`, [0]), [value("i32", 0)]);

// ./test/core/gc/array_init_data.wast:90
assert_return(() => invoke($0, `array_get_nth_i16`, [2]), [value("i32", 0)]);

// ./test/core/gc/array_init_data.wast:91
assert_return(() => invoke($0, `array_get_nth_i16`, [5]), [value("i32", 0)]);

// ./test/core/gc/array_init_data.wast:92
assert_trap(() => invoke($0, `array_get_nth_i16`, [6]), `out of bounds array access`);

// ./test/core/gc/array_init_data.wast:95
assert_return(() => invoke($0, `array_init_data`, [4, 2, 2]), []);

// ./test/core/gc/array_init_data.wast:96
assert_return(() => invoke($0, `array_get_nth`, [3]), [value("i32", 0)]);

// ./test/core/gc/array_init_data.wast:97
assert_return(() => invoke($0, `array_get_nth`, [4]), [value("i32", 99)]);

// ./test/core/gc/array_init_data.wast:98
assert_return(() => invoke($0, `array_get_nth`, [5]), [value("i32", 100)]);

// ./test/core/gc/array_init_data.wast:99
assert_return(() => invoke($0, `array_get_nth`, [6]), [value("i32", 0)]);

// ./test/core/gc/array_init_data.wast:101
assert_return(() => invoke($0, `array_init_data_i16`, [2, 5, 2]), []);

// ./test/core/gc/array_init_data.wast:102
assert_return(() => invoke($0, `array_get_nth_i16`, [1]), [value("i32", 0)]);

// ./test/core/gc/array_init_data.wast:103
assert_return(() => invoke($0, `array_get_nth_i16`, [2]), [value("i32", 26470)]);

// ./test/core/gc/array_init_data.wast:104
assert_return(() => invoke($0, `array_get_nth_i16`, [3]), [value("i32", 26984)]);

// ./test/core/gc/array_init_data.wast:105
assert_return(() => invoke($0, `array_get_nth_i16`, [4]), [value("i32", 0)]);

// ./test/core/gc/array_init_data.wast:108
assert_return(() => invoke($0, `drop_segs`, []), []);

// ./test/core/gc/array_init_data.wast:109
assert_return(() => invoke($0, `array_init_data`, [0, 0, 0]), []);

// ./test/core/gc/array_init_data.wast:110
assert_trap(() => invoke($0, `array_init_data`, [0, 0, 1]), `out of bounds memory access`);
