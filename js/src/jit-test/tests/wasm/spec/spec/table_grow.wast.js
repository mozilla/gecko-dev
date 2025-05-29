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

// ./test/core/table_grow.wast

// ./test/core/table_grow.wast:1
let $0 = instantiate(`(module
  (table $$t 0 externref)

  (func (export "get") (param $$i i32) (result externref) (table.get $$t (local.get $$i)))
  (func (export "set") (param $$i i32) (param $$r externref) (table.set $$t (local.get $$i) (local.get $$r)))

  (func (export "grow") (param $$sz i32) (param $$init externref) (result i32)
    (table.grow $$t (local.get $$init) (local.get $$sz))
  )
  (func (export "grow-abbrev") (param $$sz i32) (param $$init externref) (result i32)
    (table.grow (local.get $$init) (local.get $$sz))
  )
  (func (export "size") (result i32) (table.size $$t))

  (table $$t64 i64 0 externref)

  (func (export "get-t64") (param $$i i64) (result externref) (table.get $$t64 (local.get $$i)))
  (func (export "set-t64") (param $$i i64) (param $$r externref) (table.set $$t64 (local.get $$i) (local.get $$r)))
  (func (export "grow-t64") (param $$sz i64) (param $$init externref) (result i64)
    (table.grow $$t64 (local.get $$init) (local.get $$sz))
  )
  (func (export "size-t64") (result i64) (table.size $$t64))
)`);

// ./test/core/table_grow.wast:25
assert_return(() => invoke($0, `size`, []), [value("i32", 0)]);

// ./test/core/table_grow.wast:26
assert_trap(() => invoke($0, `set`, [0, externref(2)]), `out of bounds table access`);

// ./test/core/table_grow.wast:27
assert_trap(() => invoke($0, `get`, [0]), `out of bounds table access`);

// ./test/core/table_grow.wast:29
assert_return(() => invoke($0, `grow`, [1, null]), [value("i32", 0)]);

// ./test/core/table_grow.wast:30
assert_return(() => invoke($0, `size`, []), [value("i32", 1)]);

// ./test/core/table_grow.wast:31
assert_return(() => invoke($0, `get`, [0]), [value('externref', null)]);

// ./test/core/table_grow.wast:32
assert_return(() => invoke($0, `set`, [0, externref(2)]), []);

// ./test/core/table_grow.wast:33
assert_return(() => invoke($0, `get`, [0]), [new ExternRefResult(2)]);

// ./test/core/table_grow.wast:34
assert_trap(() => invoke($0, `set`, [1, externref(2)]), `out of bounds table access`);

// ./test/core/table_grow.wast:35
assert_trap(() => invoke($0, `get`, [1]), `out of bounds table access`);

// ./test/core/table_grow.wast:37
assert_return(() => invoke($0, `grow-abbrev`, [4, externref(3)]), [value("i32", 1)]);

// ./test/core/table_grow.wast:38
assert_return(() => invoke($0, `size`, []), [value("i32", 5)]);

// ./test/core/table_grow.wast:39
assert_return(() => invoke($0, `get`, [0]), [new ExternRefResult(2)]);

// ./test/core/table_grow.wast:40
assert_return(() => invoke($0, `set`, [0, externref(2)]), []);

// ./test/core/table_grow.wast:41
assert_return(() => invoke($0, `get`, [0]), [new ExternRefResult(2)]);

// ./test/core/table_grow.wast:42
assert_return(() => invoke($0, `get`, [1]), [new ExternRefResult(3)]);

// ./test/core/table_grow.wast:43
assert_return(() => invoke($0, `get`, [4]), [new ExternRefResult(3)]);

// ./test/core/table_grow.wast:44
assert_return(() => invoke($0, `set`, [4, externref(4)]), []);

// ./test/core/table_grow.wast:45
assert_return(() => invoke($0, `get`, [4]), [new ExternRefResult(4)]);

// ./test/core/table_grow.wast:46
assert_trap(() => invoke($0, `set`, [5, externref(2)]), `out of bounds table access`);

// ./test/core/table_grow.wast:47
assert_trap(() => invoke($0, `get`, [5]), `out of bounds table access`);

// ./test/core/table_grow.wast:50
assert_return(() => invoke($0, `size-t64`, []), [value("i64", 0n)]);

// ./test/core/table_grow.wast:51
assert_trap(() => invoke($0, `set-t64`, [0n, externref(2)]), `out of bounds table access`);

// ./test/core/table_grow.wast:52
assert_trap(() => invoke($0, `get-t64`, [0n]), `out of bounds table access`);

// ./test/core/table_grow.wast:54
assert_return(() => invoke($0, `grow-t64`, [1n, null]), [value("i64", 0n)]);

// ./test/core/table_grow.wast:55
assert_return(() => invoke($0, `size-t64`, []), [value("i64", 1n)]);

// ./test/core/table_grow.wast:56
assert_return(() => invoke($0, `get-t64`, [0n]), [value('externref', null)]);

// ./test/core/table_grow.wast:57
assert_return(() => invoke($0, `set-t64`, [0n, externref(2)]), []);

// ./test/core/table_grow.wast:58
assert_return(() => invoke($0, `get-t64`, [0n]), [new ExternRefResult(2)]);

// ./test/core/table_grow.wast:59
assert_trap(() => invoke($0, `set-t64`, [1n, externref(2)]), `out of bounds table access`);

// ./test/core/table_grow.wast:60
assert_trap(() => invoke($0, `get-t64`, [1n]), `out of bounds table access`);

// ./test/core/table_grow.wast:62
assert_return(() => invoke($0, `grow-t64`, [4n, externref(3)]), [value("i64", 1n)]);

// ./test/core/table_grow.wast:63
assert_return(() => invoke($0, `size-t64`, []), [value("i64", 5n)]);

// ./test/core/table_grow.wast:64
assert_return(() => invoke($0, `get-t64`, [0n]), [new ExternRefResult(2)]);

// ./test/core/table_grow.wast:65
assert_return(() => invoke($0, `set-t64`, [0n, externref(2)]), []);

// ./test/core/table_grow.wast:66
assert_return(() => invoke($0, `get-t64`, [0n]), [new ExternRefResult(2)]);

// ./test/core/table_grow.wast:67
assert_return(() => invoke($0, `get-t64`, [1n]), [new ExternRefResult(3)]);

// ./test/core/table_grow.wast:68
assert_return(() => invoke($0, `get-t64`, [4n]), [new ExternRefResult(3)]);

// ./test/core/table_grow.wast:69
assert_return(() => invoke($0, `set-t64`, [4n, externref(4)]), []);

// ./test/core/table_grow.wast:70
assert_return(() => invoke($0, `get-t64`, [4n]), [new ExternRefResult(4)]);

// ./test/core/table_grow.wast:71
assert_trap(() => invoke($0, `set-t64`, [5n, externref(2)]), `out of bounds table access`);

// ./test/core/table_grow.wast:72
assert_trap(() => invoke($0, `get-t64`, [5n]), `out of bounds table access`);

// ./test/core/table_grow.wast:75
let $1 = instantiate(`(module
  (table $$t 0x10 funcref)
  (elem declare func $$f)
  (func $$f (export "grow") (result i32)
    (table.grow $$t (ref.func $$f) (i32.const 0xffff_fff0))
  )
)`);

// ./test/core/table_grow.wast:83
assert_return(() => invoke($1, `grow`, []), [value("i32", -1)]);

// ./test/core/table_grow.wast:86
let $2 = instantiate(`(module
  (table $$t 0 externref)
  (func (export "grow") (param i32) (result i32)
    (table.grow $$t (ref.null extern) (local.get 0))
  )
)`);

// ./test/core/table_grow.wast:93
assert_return(() => invoke($2, `grow`, [0]), [value("i32", 0)]);

// ./test/core/table_grow.wast:94
assert_return(() => invoke($2, `grow`, [1]), [value("i32", 0)]);

// ./test/core/table_grow.wast:95
assert_return(() => invoke($2, `grow`, [0]), [value("i32", 1)]);

// ./test/core/table_grow.wast:96
assert_return(() => invoke($2, `grow`, [2]), [value("i32", 1)]);

// ./test/core/table_grow.wast:97
assert_return(() => invoke($2, `grow`, [800]), [value("i32", 3)]);

// ./test/core/table_grow.wast:100
let $3 = instantiate(`(module
  (table $$t 0 10 externref)
  (func (export "grow") (param i32) (result i32)
    (table.grow $$t (ref.null extern) (local.get 0))
  )
)`);

// ./test/core/table_grow.wast:107
assert_return(() => invoke($3, `grow`, [0]), [value("i32", 0)]);

// ./test/core/table_grow.wast:108
assert_return(() => invoke($3, `grow`, [1]), [value("i32", 0)]);

// ./test/core/table_grow.wast:109
assert_return(() => invoke($3, `grow`, [1]), [value("i32", 1)]);

// ./test/core/table_grow.wast:110
assert_return(() => invoke($3, `grow`, [2]), [value("i32", 2)]);

// ./test/core/table_grow.wast:111
assert_return(() => invoke($3, `grow`, [6]), [value("i32", 4)]);

// ./test/core/table_grow.wast:112
assert_return(() => invoke($3, `grow`, [0]), [value("i32", 10)]);

// ./test/core/table_grow.wast:113
assert_return(() => invoke($3, `grow`, [1]), [value("i32", -1)]);

// ./test/core/table_grow.wast:114
assert_return(() => invoke($3, `grow`, [65536]), [value("i32", -1)]);

// ./test/core/table_grow.wast:117
let $4 = instantiate(`(module
  (table $$t 10 funcref)
  (func (export "grow") (param i32) (result i32)
    (table.grow $$t (ref.null func) (local.get 0))
  )
  (elem declare func 1)
  (func (export "check-table-null") (param i32 i32) (result funcref)
    (local funcref)
    (local.set 2 (ref.func 1))
    (block
      (loop
        (local.set 2 (table.get $$t (local.get 0)))
        (br_if 1 (i32.eqz (ref.is_null (local.get 2))))
        (br_if 1 (i32.ge_u (local.get 0) (local.get 1)))
        (local.set 0 (i32.add (local.get 0) (i32.const 1)))
        (br_if 0 (i32.le_u (local.get 0) (local.get 1)))
      )
    )
    (local.get 2)
  )
)`);

// ./test/core/table_grow.wast:139
assert_return(() => invoke($4, `check-table-null`, [0, 9]), [value('anyfunc', null)]);

// ./test/core/table_grow.wast:140
assert_return(() => invoke($4, `grow`, [10]), [value("i32", 10)]);

// ./test/core/table_grow.wast:141
assert_return(() => invoke($4, `check-table-null`, [0, 19]), [value('anyfunc', null)]);

// ./test/core/table_grow.wast:144
let $5 = instantiate(`(module $$Tgt
  (table (export "table") 1 funcref) ;; initial size is 1
  (func (export "grow") (result i32) (table.grow (ref.null func) (i32.const 1)))
)`);
let $Tgt = $5;

// ./test/core/table_grow.wast:148
register($Tgt, `grown-table`);

// ./test/core/table_grow.wast:149
assert_return(() => invoke($Tgt, `grow`, []), [value("i32", 1)]);

// ./test/core/table_grow.wast:150
let $6 = instantiate(`(module $$Tgit1
  ;; imported table limits should match, because external table size is 2 now
  (table (export "table") (import "grown-table" "table") 2 funcref)
  (func (export "grow") (result i32) (table.grow (ref.null func) (i32.const 1)))
)`);
let $Tgit1 = $6;

// ./test/core/table_grow.wast:155
register($Tgit1, `grown-imported-table`);

// ./test/core/table_grow.wast:156
assert_return(() => invoke($Tgit1, `grow`, []), [value("i32", 2)]);

// ./test/core/table_grow.wast:157
let $7 = instantiate(`(module $$Tgit2
  ;; imported table limits should match, because external table size is 3 now
  (import "grown-imported-table" "table" (table 3 funcref))
  (func (export "size") (result i32) (table.size))
)`);
let $Tgit2 = $7;

// ./test/core/table_grow.wast:162
assert_return(() => invoke($Tgit2, `size`, []), [value("i32", 3)]);

// ./test/core/table_grow.wast:167
assert_invalid(
  () => instantiate(`(module
    (table $$t 0 externref)
    (func $$type-init-size-empty-vs-i32-externref (result i32)
      (table.grow $$t)
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_grow.wast:176
assert_invalid(
  () => instantiate(`(module
    (table $$t 0 externref)
    (func $$type-size-empty-vs-i32 (result i32)
      (table.grow $$t (ref.null extern))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_grow.wast:185
assert_invalid(
  () => instantiate(`(module
    (table $$t 0 externref)
    (func $$type-init-empty-vs-externref (result i32)
      (table.grow $$t (i32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_grow.wast:194
assert_invalid(
  () => instantiate(`(module
    (table $$t 0 externref)
    (func $$type-size-f32-vs-i32 (result i32)
      (table.grow $$t (ref.null extern) (f32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_grow.wast:203
assert_invalid(
  () => instantiate(`(module
    (table $$t 0 funcref)
    (func $$type-init-externref-vs-funcref (param $$r externref) (result i32)
      (table.grow $$t (local.get $$r) (i32.const 1))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_grow.wast:213
assert_invalid(
  () => instantiate(`(module
    (table $$t 1 externref)
    (func $$type-result-i32-vs-empty
      (table.grow $$t (ref.null extern) (i32.const 0))
    )
  )`),
  `type mismatch`,
);

// ./test/core/table_grow.wast:222
assert_invalid(
  () => instantiate(`(module
    (table $$t 1 externref)
    (func $$type-result-i32-vs-f32 (result f32)
      (table.grow $$t (ref.null extern) (i32.const 0))
    )
  )`),
  `type mismatch`,
);
