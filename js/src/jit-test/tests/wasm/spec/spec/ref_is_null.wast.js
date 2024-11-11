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

// ./test/core/ref_is_null.wast

// ./test/core/ref_is_null.wast:1
let $0 = instantiate(`(module
  (type $$t (func))
  (func $$dummy)

  (func $$f1 (export "funcref") (param $$x funcref) (result i32)
    (ref.is_null (local.get $$x))
  )
  (func $$f2 (export "externref") (param $$x externref) (result i32)
    (ref.is_null (local.get $$x))
  )
  (func $$f3 (param $$x (ref null $$t)) (result i32)
    (ref.is_null (local.get $$x))
  )
  (func $$f3' (export "ref-null") (result i32)
    (call $$f3 (ref.null $$t))
  )

  (table $$t1 2 funcref)
  (table $$t2 2 externref)
  (table $$t3 2 (ref null $$t))
  (elem (table $$t1) (i32.const 1) func $$dummy)
  (elem (table $$t3) (i32.const 1) (ref $$t) (ref.func $$dummy))

  (func (export "init") (param $$r externref)
    (table.set $$t2 (i32.const 1) (local.get $$r))
  )
  (func (export "deinit")
    (table.set $$t1 (i32.const 1) (ref.null func))
    (table.set $$t2 (i32.const 1) (ref.null extern))
    (table.set $$t3 (i32.const 1) (ref.null $$t))
  )

  (func (export "funcref-elem") (param $$x i32) (result i32)
    (call $$f1 (table.get $$t1 (local.get $$x)))
  )
  (func (export "externref-elem") (param $$x i32) (result i32)
    (call $$f2 (table.get $$t2 (local.get $$x)))
  )
  (func (export "ref-elem") (param $$x i32) (result i32)
    (call $$f3 (table.get $$t3 (local.get $$x)))
  )
)`);

// ./test/core/ref_is_null.wast:44
assert_return(() => invoke($0, `funcref`, [null]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:45
assert_return(() => invoke($0, `externref`, [null]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:46
assert_return(() => invoke($0, `ref-null`, []), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:48
assert_return(() => invoke($0, `externref`, [externref(1)]), [value("i32", 0)]);

// ./test/core/ref_is_null.wast:50
invoke($0, `init`, [externref(0)]);

// ./test/core/ref_is_null.wast:52
assert_return(() => invoke($0, `funcref-elem`, [0]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:53
assert_return(() => invoke($0, `externref-elem`, [0]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:54
assert_return(() => invoke($0, `ref-elem`, [0]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:56
assert_return(() => invoke($0, `funcref-elem`, [1]), [value("i32", 0)]);

// ./test/core/ref_is_null.wast:57
assert_return(() => invoke($0, `externref-elem`, [1]), [value("i32", 0)]);

// ./test/core/ref_is_null.wast:58
assert_return(() => invoke($0, `ref-elem`, [1]), [value("i32", 0)]);

// ./test/core/ref_is_null.wast:60
invoke($0, `deinit`, []);

// ./test/core/ref_is_null.wast:62
assert_return(() => invoke($0, `funcref-elem`, [0]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:63
assert_return(() => invoke($0, `externref-elem`, [0]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:64
assert_return(() => invoke($0, `ref-elem`, [0]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:66
assert_return(() => invoke($0, `funcref-elem`, [1]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:67
assert_return(() => invoke($0, `externref-elem`, [1]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:68
assert_return(() => invoke($0, `ref-elem`, [1]), [value("i32", 1)]);

// ./test/core/ref_is_null.wast:71
let $1 = instantiate(`(module
  (type $$t (func))
  (func (param $$r (ref $$t)) (drop (ref.is_null (local.get $$r))))
  (func (param $$r (ref func)) (drop (ref.is_null (local.get $$r))))
  (func (param $$r (ref extern)) (drop (ref.is_null (local.get $$r))))
)`);

// ./test/core/ref_is_null.wast:78
assert_invalid(
  () => instantiate(`(module (func $$ref-vs-num (param i32) (ref.is_null (local.get 0))))`),
  `type mismatch`,
);

// ./test/core/ref_is_null.wast:82
assert_invalid(
  () => instantiate(`(module (func $$ref-vs-empty (ref.is_null)))`),
  `type mismatch`,
);
