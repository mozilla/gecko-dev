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

// ./test/core/gc/array_new_elem.wast

// ./test/core/gc/array_new_elem.wast:3
let $0 = instantiate(`(module
  (type $$arr (array i31ref))

  (elem $$e i31ref
    (ref.i31 (i32.const 0xaa))
    (ref.i31 (i32.const 0xbb))
    (ref.i31 (i32.const 0xcc))
    (ref.i31 (i32.const 0xdd)))

  (func (export "array-new-elem") (param i32 i32) (result (ref $$arr))
    (array.new_elem $$arr $$e (local.get 0) (local.get 1))
  )
)`);

// ./test/core/gc/array_new_elem.wast:18
assert_return(() => invoke($0, `array-new-elem`, [0, 0]), [new RefWithType('arrayref')]);

// ./test/core/gc/array_new_elem.wast:19
assert_return(() => invoke($0, `array-new-elem`, [0, 4]), [new RefWithType('arrayref')]);

// ./test/core/gc/array_new_elem.wast:20
assert_return(() => invoke($0, `array-new-elem`, [1, 2]), [new RefWithType('arrayref')]);

// ./test/core/gc/array_new_elem.wast:21
assert_return(() => invoke($0, `array-new-elem`, [4, 0]), [new RefWithType('arrayref')]);

// ./test/core/gc/array_new_elem.wast:24
assert_trap(() => invoke($0, `array-new-elem`, [0, 5]), `out of bounds table access`);

// ./test/core/gc/array_new_elem.wast:25
assert_trap(() => invoke($0, `array-new-elem`, [5, 0]), `out of bounds table access`);

// ./test/core/gc/array_new_elem.wast:26
assert_trap(() => invoke($0, `array-new-elem`, [1, 4]), `out of bounds table access`);

// ./test/core/gc/array_new_elem.wast:27
assert_trap(() => invoke($0, `array-new-elem`, [4, 1]), `out of bounds table access`);

// ./test/core/gc/array_new_elem.wast:29
let $1 = instantiate(`(module
  (type $$arr (array i31ref))

  (elem $$e i31ref
    (ref.i31 (i32.const 0xaa))
    (ref.i31 (i32.const 0xbb))
    (ref.i31 (i32.const 0xcc))
    (ref.i31 (i32.const 0xdd)))

  (func (export "array-new-elem-contents") (result i32 i32)
    (local (ref $$arr))
    (local.set 0 (array.new_elem $$arr $$e (i32.const 1) (i32.const 2)))
    (i31.get_u (array.get $$arr (local.get 0) (i32.const 0)))
    (i31.get_u (array.get $$arr (local.get 0) (i32.const 1)))
  )
)`);

// ./test/core/gc/array_new_elem.wast:47
assert_return(() => invoke($1, `array-new-elem-contents`, []), [value("i32", 187), value("i32", 204)]);

// ./test/core/gc/array_new_elem.wast:51
let $2 = instantiate(`(module
  (type $$arr (array funcref))

  (elem $$e func $$aa $$bb $$cc $$dd)
  (func $$aa (result i32) (i32.const 0xaa))
  (func $$bb (result i32) (i32.const 0xbb))
  (func $$cc (result i32) (i32.const 0xcc))
  (func $$dd (result i32) (i32.const 0xdd))

  (func (export "array-new-elem") (param i32 i32) (result (ref $$arr))
    (array.new_elem $$arr $$e (local.get 0) (local.get 1))
  )
)`);

// ./test/core/gc/array_new_elem.wast:66
assert_return(() => invoke($2, `array-new-elem`, [0, 0]), [new RefWithType('arrayref')]);

// ./test/core/gc/array_new_elem.wast:67
assert_return(() => invoke($2, `array-new-elem`, [0, 4]), [new RefWithType('arrayref')]);

// ./test/core/gc/array_new_elem.wast:68
assert_return(() => invoke($2, `array-new-elem`, [1, 2]), [new RefWithType('arrayref')]);

// ./test/core/gc/array_new_elem.wast:69
assert_return(() => invoke($2, `array-new-elem`, [4, 0]), [new RefWithType('arrayref')]);

// ./test/core/gc/array_new_elem.wast:72
assert_trap(() => invoke($2, `array-new-elem`, [0, 5]), `out of bounds table access`);

// ./test/core/gc/array_new_elem.wast:73
assert_trap(() => invoke($2, `array-new-elem`, [5, 0]), `out of bounds table access`);

// ./test/core/gc/array_new_elem.wast:74
assert_trap(() => invoke($2, `array-new-elem`, [1, 4]), `out of bounds table access`);

// ./test/core/gc/array_new_elem.wast:75
assert_trap(() => invoke($2, `array-new-elem`, [4, 1]), `out of bounds table access`);

// ./test/core/gc/array_new_elem.wast:77
let $3 = instantiate(`(module
  (type $$f (func (result i32)))
  (type $$arr (array funcref))

  (elem $$e func $$aa $$bb $$cc $$dd)
  (func $$aa (result i32) (i32.const 0xaa))
  (func $$bb (result i32) (i32.const 0xbb))
  (func $$cc (result i32) (i32.const 0xcc))
  (func $$dd (result i32) (i32.const 0xdd))

  (table $$t 2 2 funcref)

  (func (export "array-new-elem-contents") (result i32 i32)
    (local (ref $$arr))
    (local.set 0 (array.new_elem $$arr $$e (i32.const 1) (i32.const 2)))

    (table.set $$t (i32.const 0) (array.get $$arr (local.get 0) (i32.const 0)))
    (table.set $$t (i32.const 1) (array.get $$arr (local.get 0) (i32.const 1)))

    (call_indirect (type $$f) (i32.const 0))
    (call_indirect (type $$f) (i32.const 1))

  )
)`);

// ./test/core/gc/array_new_elem.wast:103
assert_return(() => invoke($3, `array-new-elem-contents`, []), [value("i32", 187), value("i32", 204)]);
