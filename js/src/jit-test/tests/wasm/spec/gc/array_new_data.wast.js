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

// ./test/core/gc/array_new_data.wast:24
let $1 = instantiate(`(module
  (type $$arr (array (mut i8)))

  (data $$d "\\aa\\bb\\cc\\dd")

  (func (export "array-new-data-contents") (result i32 i32)
    (local (ref $$arr))
    (local.set 0 (array.new_data $$arr $$d (i32.const 1) (i32.const 2)))
    (array.get_u $$arr (local.get 0) (i32.const 0))
    (array.get_u $$arr (local.get 0) (i32.const 1))
  )
)`);

// ./test/core/gc/array_new_data.wast:38
assert_return(() => invoke($1, `array-new-data-contents`, []), [value("i32", 187), value("i32", 204)]);

// ./test/core/gc/array_new_data.wast:40
let $2 = instantiate(`(module
  (type $$arr (array (mut i32)))

  (data $$d "\\aa\\bb\\cc\\dd")

  (func (export "array-new-data-little-endian") (result i32)
    (array.get $$arr
               (array.new_data $$arr $$d (i32.const 0) (i32.const 1))
               (i32.const 0))
  )
)`);

// ./test/core/gc/array_new_data.wast:53
assert_return(() => invoke($2, `array-new-data-little-endian`, []), [value("i32", -573785174)]);

// ./test/core/gc/array_new_data.wast:55
let $3 = instantiate(`(module
  (type $$arr (array (mut i16)))

  (data $$d "\\00\\11\\22")

  (func (export "array-new-data-unaligned") (result i32)
    (array.get_u $$arr
                 (array.new_data $$arr $$d (i32.const 1) (i32.const 1))
                 (i32.const 0))
  )
)`);

// ./test/core/gc/array_new_data.wast:68
assert_return(() => invoke($3, `array-new-data-unaligned`, []), [value("i32", 8721)]);
