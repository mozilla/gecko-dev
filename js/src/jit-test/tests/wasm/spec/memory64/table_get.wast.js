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

// ./test/core/table_get.wast

// ./test/core/table_get.wast:1
let $0 = instantiate(`(module
  (table $$t2 2 externref)
  (table $$t3 3 funcref)
  (table $$t64 i64 3 funcref)
  (elem (table $$t3) (i32.const 1) func $$dummy)
  (func $$dummy)

  (func (export "init") (param $$r externref)
    (table.set $$t2 (i32.const 1) (local.get $$r))
    (table.set $$t3 (i32.const 2) (table.get $$t3 (i32.const 1)))
  )

  (func (export "get-externref") (param $$i i32) (result externref)
    (table.get (local.get $$i))
  )
  (func $$f3 (export "get-funcref") (param $$i i32) (result funcref)
    (table.get $$t3 (local.get $$i))
  )
  (func $$f4 (export "get-funcref-t64") (param $$i i64) (result funcref)
    (table.get $$t64 (local.get $$i))
  )

  (func (export "is_null-funcref") (param $$i i32) (result i32)
    (ref.is_null (call $$f3 (local.get $$i)))
  )
)`);

// ./test/core/table_get.wast:28
invoke($0, `init`, [externref(1)]);

// ./test/core/table_get.wast:30
assert_return(() => invoke($0, `get-externref`, [0]), [value('externref', null)]);

// ./test/core/table_get.wast:31
assert_return(() => invoke($0, `get-externref`, [1]), [new ExternRefResult(1)]);

// ./test/core/table_get.wast:33
assert_return(() => invoke($0, `get-funcref`, [0]), [value('anyfunc', null)]);

// ./test/core/table_get.wast:34
assert_return(() => invoke($0, `get-funcref-t64`, [0n]), [value('anyfunc', null)]);

// ./test/core/table_get.wast:35
assert_return(() => invoke($0, `is_null-funcref`, [1]), [value("i32", 0)]);

// ./test/core/table_get.wast:36
assert_return(() => invoke($0, `is_null-funcref`, [2]), [value("i32", 0)]);

// ./test/core/table_get.wast:38
assert_trap(() => invoke($0, `get-externref`, [2]), `out of bounds table access`);

// ./test/core/table_get.wast:39
assert_trap(() => invoke($0, `get-funcref`, [3]), `out of bounds table access`);

// ./test/core/table_get.wast:40
assert_trap(() => invoke($0, `get-externref`, [-1]), `out of bounds table access`);

// ./test/core/table_get.wast:41
assert_trap(() => invoke($0, `get-funcref`, [-1]), `out of bounds table access`);

// ./test/core/table_get.wast:46
assert_invalid(
  () => instantiate(`(module
    (table $$t 10 externref)
    (func $$type-index-empty-vs-i32 (result externref)
      (table.get $$t)
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_get.wast:55
assert_invalid(
  () => instantiate(`(module
    (table $$t 10 externref)
    (func $$type-index-f32-vs-i32 (result externref)
      (table.get $$t (f32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_get.wast:65
assert_invalid(
  () => instantiate(`(module
    (table $$t 10 externref)
    (func $$type-result-externref-vs-empty
      (table.get $$t (i32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_get.wast:74
assert_invalid(
  () => instantiate(`(module
    (table $$t 10 externref)
    (func $$type-result-externref-vs-funcref (result funcref)
      (table.get $$t (i32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_get.wast:84
assert_invalid(
  () => instantiate(`(module
    (table $$t1 1 funcref)
    (table $$t2 1 externref)
    (func $$type-result-externref-vs-funcref-multi (result funcref)
      (table.get $$t2 (i32.const 0))
    )
  )`),
  `type mismatch`,
);
