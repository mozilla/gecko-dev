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

// ./test/core/gc/array_init_elem.wast

// ./test/core/gc/array_init_elem.wast:5
assert_invalid(
  () => instantiate(`(module
    (type $$a (array funcref))

    (elem $$e1 funcref)

    (func (export "array.init_elem-immutable") (param $$1 (ref $$a))
      (array.init_elem $$a $$e1 (local.get $$1) (i32.const 0) (i32.const 0) (i32.const 0))
    )
  )`),
  `array is immutable`,
);

// ./test/core/gc/array_init_elem.wast:18
assert_invalid(
  () => instantiate(`(module
    (type $$a (array (mut i8)))

    (elem $$e1 funcref)

    (func (export "array.init_elem-invalid-1") (param $$1 (ref $$a))
      (array.init_elem $$a $$e1 (local.get $$1) (i32.const 0) (i32.const 0) (i32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/array_init_elem.wast:31
assert_invalid(
  () => instantiate(`(module
    (type $$a (array (mut funcref)))

    (elem $$e1 externref)

    (func (export "array.init_elem-invalid-2") (param $$1 (ref $$a))
      (array.init_elem $$a $$e1 (local.get $$1) (i32.const 0) (i32.const 0) (i32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/gc/array_init_elem.wast:44
let $0 = instantiate(`(module
  (type $$t_f (func))
  (type $$arrref (array (ref $$t_f)))
  (type $$arrref_mut (array (mut funcref)))

  (global $$g_arrref (ref $$arrref) (array.new $$arrref (ref.func $$dummy) (i32.const 12)))
  (global $$g_arrref_mut (ref $$arrref_mut) (array.new_default $$arrref_mut (i32.const 12)))

  (table $$t 1 funcref)

  (elem $$e1 func $$dummy $$dummy $$dummy $$dummy $$dummy $$dummy $$dummy $$dummy $$dummy $$dummy $$dummy $$dummy)

  (func $$dummy
  )

  (func (export "array_call_nth") (param $$1 i32)
    (table.set $$t (i32.const 0) (array.get $$arrref_mut (global.get $$g_arrref_mut) (local.get $$1)))
    (call_indirect $$t (i32.const 0))
  )

  (func (export "array_init_elem-null")
    (array.init_elem $$arrref_mut $$e1 (ref.null $$arrref_mut) (i32.const 0) (i32.const 0) (i32.const 0))
  )

  (func (export "array_init_elem") (param $$1 i32) (param $$2 i32) (param $$3 i32)
    (array.init_elem $$arrref_mut $$e1 (global.get $$g_arrref_mut) (local.get $$1) (local.get $$2) (local.get $$3))
  )

  (func (export "drop_segs")
    (elem.drop $$e1)
  )
)`);

// ./test/core/gc/array_init_elem.wast:78
assert_trap(() => invoke($0, `array_init_elem-null`, []), `null array reference`);

// ./test/core/gc/array_init_elem.wast:81
assert_trap(() => invoke($0, `array_init_elem`, [13, 0, 0]), `out of bounds array access`);

// ./test/core/gc/array_init_elem.wast:82
assert_trap(() => invoke($0, `array_init_elem`, [0, 13, 0]), `out of bounds table access`);

// ./test/core/gc/array_init_elem.wast:85
assert_trap(() => invoke($0, `array_init_elem`, [0, 0, 13]), `out of bounds array access`);

// ./test/core/gc/array_init_elem.wast:86
assert_trap(() => invoke($0, `array_init_elem`, [0, 0, 13]), `out of bounds array access`);

// ./test/core/gc/array_init_elem.wast:89
assert_return(() => invoke($0, `array_init_elem`, [12, 0, 0]), []);

// ./test/core/gc/array_init_elem.wast:90
assert_return(() => invoke($0, `array_init_elem`, [0, 12, 0]), []);

// ./test/core/gc/array_init_elem.wast:93
assert_trap(() => invoke($0, `array_call_nth`, [0]), `uninitialized element`);

// ./test/core/gc/array_init_elem.wast:94
assert_trap(() => invoke($0, `array_call_nth`, [5]), `uninitialized element`);

// ./test/core/gc/array_init_elem.wast:95
assert_trap(() => invoke($0, `array_call_nth`, [11]), `uninitialized element`);

// ./test/core/gc/array_init_elem.wast:96
assert_trap(() => invoke($0, `array_call_nth`, [12]), `out of bounds array access`);

// ./test/core/gc/array_init_elem.wast:99
assert_return(() => invoke($0, `array_init_elem`, [2, 3, 2]), []);

// ./test/core/gc/array_init_elem.wast:100
assert_trap(() => invoke($0, `array_call_nth`, [1]), `uninitialized element`);

// ./test/core/gc/array_init_elem.wast:101
assert_return(() => invoke($0, `array_call_nth`, [2]), []);

// ./test/core/gc/array_init_elem.wast:102
assert_return(() => invoke($0, `array_call_nth`, [3]), []);

// ./test/core/gc/array_init_elem.wast:103
assert_trap(() => invoke($0, `array_call_nth`, [4]), `uninitialized element`);

// ./test/core/gc/array_init_elem.wast:106
assert_return(() => invoke($0, `drop_segs`, []), []);

// ./test/core/gc/array_init_elem.wast:107
assert_return(() => invoke($0, `array_init_elem`, [0, 0, 0]), []);

// ./test/core/gc/array_init_elem.wast:108
assert_trap(() => invoke($0, `array_init_elem`, [0, 0, 1]), `out of bounds table access`);
