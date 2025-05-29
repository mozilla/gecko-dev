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

// ./test/core/table.wast

// ./test/core/table.wast:3
let $0 = instantiate(`(module (table 0 funcref))`);

// ./test/core/table.wast:4
let $1 = instantiate(`(module (table 1 funcref))`);

// ./test/core/table.wast:5
let $2 = instantiate(`(module (table 0 0 funcref))`);

// ./test/core/table.wast:6
let $3 = instantiate(`(module (table 0 1 funcref))`);

// ./test/core/table.wast:7
let $4 = instantiate(`(module (table 1 256 funcref))`);

// ./test/core/table.wast:8
let $5 = instantiate(`(module (table 0 65536 funcref))`);

// ./test/core/table.wast:9
let _anon_8 = module(`(module (table 0xffff_ffff funcref))`);

// ./test/core/table.wast:10
let $6 = instantiate(`(module (table 0 0xffff_ffff funcref))`);

// ./test/core/table.wast:12
let $7 = instantiate(`(module (table 1 (ref null func)))`);

// ./test/core/table.wast:13
let $8 = instantiate(`(module (table 1 (ref null extern)))`);

// ./test/core/table.wast:14
let $9 = instantiate(`(module (table 1 (ref null $$t)) (type $$t (func)))`);

// ./test/core/table.wast:16
let $10 = instantiate(`(module (table 0 funcref) (table 0 funcref))`);

// ./test/core/table.wast:17
let $11 = instantiate(`(module (table (import "spectest" "table") 0 funcref) (table 0 funcref))`);

// ./test/core/table.wast:19
let $12 = instantiate(`(module (table 0 funcref (ref.null func)))`);

// ./test/core/table.wast:20
let $13 = instantiate(`(module (table 1 funcref (ref.null func)))`);

// ./test/core/table.wast:21
let $14 = instantiate(`(module (table 1 (ref null func) (ref.null func)))`);

// ./test/core/table.wast:23
assert_invalid(() => instantiate(`(module (elem (i32.const 0)))`), `unknown table`);

// ./test/core/table.wast:24
assert_invalid(
  () => instantiate(`(module (elem (i32.const 0) $$f) (func $$f))`),
  `unknown table`,
);

// ./test/core/table.wast:26
assert_invalid(
  () => instantiate(`(module (table 1 0 funcref))`),
  `size minimum must not be greater than maximum`,
);

// ./test/core/table.wast:30
assert_invalid(
  () => instantiate(`(module (table 0xffff_ffff 0 funcref))`),
  `size minimum must not be greater than maximum`,
);

// Suppressed because wasm-tools cannot parse these offsets.
// // ./test/core/table.wast:35
// assert_invalid(() => instantiate(`(table 0x1_0000_0000 funcref) `), `table size`);
//
// // ./test/core/table.wast:39
// assert_invalid(
//   () => instantiate(`(table 0x1_0000_0000 0x1_0000_0000 funcref) `),
//   `table size`,
// );
//
// // ./test/core/table.wast:43
// assert_invalid(() => instantiate(`(table 0 0x1_0000_0000 funcref) `), `table size`);

// ./test/core/table.wast:50
let $15 = instantiate(`(module (table i64 0 funcref))`);

// ./test/core/table.wast:51
let $16 = instantiate(`(module (table i64 1 funcref))`);

// ./test/core/table.wast:52
let $17 = instantiate(`(module (table i64 0 0 funcref))`);

// ./test/core/table.wast:53
let $18 = instantiate(`(module (table i64 0 1 funcref))`);

// ./test/core/table.wast:54
let $19 = instantiate(`(module (table i64 1 256 funcref))`);

// ./test/core/table.wast:55
let $20 = instantiate(`(module (table i64 0 65536 funcref))`);

// ./test/core/table.wast:56
let $21 = instantiate(`(module (table i64 0 0xffff_ffff funcref))`);

// ./test/core/table.wast:57
let $22 = instantiate(`(module (table i64 0 0x1_0000_0000 funcref))`);

// ./test/core/table.wast:58
let _anon_57 = module(`(module (table i64 0xffff_ffff_ffff_ffff funcref))`);

// ./test/core/table.wast:59
let $23 = instantiate(`(module (table i64 0 0xffff_ffff_ffff_ffff funcref))`);

// ./test/core/table.wast:61
let $24 = instantiate(`(module (table i64 0 funcref) (table i64 0 funcref))`);

// ./test/core/table.wast:62
let $25 = instantiate(`(module (table (import "spectest" "table64") i64 0 funcref) (table i64 0 funcref))`);

// ./test/core/table.wast:64
assert_invalid(
  () => instantiate(`(module (table i64 1 0 funcref))`),
  `size minimum must not be greater than maximum`,
);

// ./test/core/table.wast:68
assert_invalid(
  () => instantiate(`(module (table i64 0xffff_ffff 0 funcref))`),
  `size minimum must not be greater than maximum`,
);

// ./test/core/table.wast:75
assert_invalid(() => instantiate(`(module (elem (i32.const 0)))`), `unknown table`);

// ./test/core/table.wast:76
assert_invalid(
  () => instantiate(`(module (elem (i32.const 0) $$f) (func $$f))`),
  `unknown table`,
);

// ./test/core/table.wast:78
assert_invalid(
  () => instantiate(`(module (table 1 (ref null func) (i32.const 0)))`),
  `type mismatch`,
);

// ./test/core/table.wast:82
assert_invalid(
  () => instantiate(`(module (table 1 (ref func) (ref.null extern)))`),
  `type mismatch`,
);

// ./test/core/table.wast:86
assert_invalid(
  () => instantiate(`(module (type $$t (func)) (table 1 (ref $$t) (ref.null func)))`),
  `type mismatch`,
);

// ./test/core/table.wast:90
assert_invalid(
  () => instantiate(`(module (table 1 (ref func) (ref.null func)))`),
  `type mismatch`,
);

// ./test/core/table.wast:94
assert_invalid(() => instantiate(`(module (table 0 (ref func)))`), `type mismatch`);

// ./test/core/table.wast:98
assert_invalid(() => instantiate(`(module (table 0 (ref extern)))`), `type mismatch`);

// ./test/core/table.wast:102
assert_invalid(
  () => instantiate(`(module (type $$t (func)) (table 0 (ref $$t)))`),
  `type mismatch`,
);

// ./test/core/table.wast:110
let $26 = instantiate(`(module
  (global (export "g") (ref $$f) (ref.func $$f))
  (type $$f (func))
  (func $$f)
)`);

// ./test/core/table.wast:115
register($26, `M`);

// ./test/core/table.wast:117
let $27 = instantiate(`(module
  (global $$g (import "M" "g") (ref $$dummy))

  (type $$dummy (func))
  (func $$dummy)

  (table $$t1 10 funcref)
  (table $$t2 10 funcref (ref.func $$dummy))
  (table $$t3 10 (ref $$dummy) (ref.func $$dummy))
  (table $$t4 10 funcref (global.get $$g))
  (table $$t5 10 (ref $$dummy) (global.get $$g))

  (func (export "get1") (result funcref) (table.get $$t1 (i32.const 1)))
  (func (export "get2") (result funcref) (table.get $$t2 (i32.const 4)))
  (func (export "get3") (result funcref) (table.get $$t3 (i32.const 7)))
  (func (export "get4") (result funcref) (table.get $$t4 (i32.const 8)))
  (func (export "get5") (result funcref) (table.get $$t5 (i32.const 9)))
)`);

// ./test/core/table.wast:136
assert_return(() => invoke($27, `get1`, []), [null]);

// ./test/core/table.wast:137
assert_return(() => invoke($27, `get2`, []), [new RefWithType('funcref')]);

// ./test/core/table.wast:138
assert_return(() => invoke($27, `get3`, []), [new RefWithType('funcref')]);

// ./test/core/table.wast:139
assert_return(() => invoke($27, `get4`, []), [new RefWithType('funcref')]);

// ./test/core/table.wast:140
assert_return(() => invoke($27, `get5`, []), [new RefWithType('funcref')]);

// ./test/core/table.wast:143
assert_invalid(
  () => instantiate(`(module
    (type $$f (func))
    (table 10 (ref $$f))
  )`),
  `type mismatch`,
);

// ./test/core/table.wast:151
assert_invalid(
  () => instantiate(`(module
    (type $$f (func))
    (table 0 (ref $$f))
  )`),
  `type mismatch`,
);

// ./test/core/table.wast:159
assert_invalid(
  () => instantiate(`(module
    (type $$f (func))
    (table 0 0 (ref $$f))
  )`),
  `type mismatch`,
);

// ./test/core/table.wast:170
assert_malformed(
  () => instantiate(`(table $$foo 1 funcref) (table $$foo 1 funcref) `),
  `duplicate table`,
);

// ./test/core/table.wast:177
assert_malformed(
  () => instantiate(`(import "" "" (table $$foo 1 funcref)) (table $$foo 1 funcref) `),
  `duplicate table`,
);

// ./test/core/table.wast:184
assert_malformed(
  () => instantiate(`(import "" "" (table $$foo 1 funcref)) (import "" "" (table $$foo 1 funcref)) `),
  `duplicate table`,
);
