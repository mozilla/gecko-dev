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

// ./test/core/exports.wast

// ./test/core/exports.wast:3
let $0 = instantiate(`(module (func) (export "a" (func 0)))`);

// ./test/core/exports.wast:4
let $1 = instantiate(`(module (func) (export "a" (func 0)) (export "b" (func 0)))`);

// ./test/core/exports.wast:5
let $2 = instantiate(`(module (func) (func) (export "a" (func 0)) (export "b" (func 1)))`);

// ./test/core/exports.wast:7
let $3 = instantiate(`(module (func (export "a")))`);

// ./test/core/exports.wast:8
let $4 = instantiate(`(module (func (export "a") (export "b") (export "c")))`);

// ./test/core/exports.wast:9
let $5 = instantiate(`(module (func (export "a") (export "b") (param i32)))`);

// ./test/core/exports.wast:10
let $6 = instantiate(`(module (func) (export "a" (func 0)))`);

// ./test/core/exports.wast:11
let $7 = instantiate(`(module (func $$a (export "a")))`);

// ./test/core/exports.wast:12
let $8 = instantiate(`(module (func $$a) (export "a" (func $$a)))`);

// ./test/core/exports.wast:13
let $9 = instantiate(`(module (export "a" (func 0)) (func))`);

// ./test/core/exports.wast:14
let $10 = instantiate(`(module (export "a" (func $$a)) (func $$a))`);

// ./test/core/exports.wast:16
let $11 = instantiate(`(module $$Func
  (export "e" (func $$f))
  (func $$f (param $$n i32) (result i32)
    (return (i32.add (local.get $$n) (i32.const 1)))
  )
)`);
let $Func = $11;

// ./test/core/exports.wast:22
assert_return(() => invoke($11, `e`, [42]), [value("i32", 43)]);

// ./test/core/exports.wast:23
assert_return(() => invoke($Func, `e`, [42]), [value("i32", 43)]);

// ./test/core/exports.wast:24
let $12 = instantiate(`(module)`);

// ./test/core/exports.wast:25
let $13 = instantiate(`(module $$Other1)`);
let $Other1 = $13;

// ./test/core/exports.wast:26
assert_return(() => invoke($Func, `e`, [42]), [value("i32", 43)]);

// ./test/core/exports.wast:28
let $14 = instantiate(`(module
  (type (;0;) (func (result i32)))
  (func (;0;) (type 0) (result i32) i32.const 42)
  (export "a" (func 0))
  (export "b" (func 0))
  (export "c" (func 0)))`);

// ./test/core/exports.wast:34
assert_return(() => invoke($14, `a`, []), [value("i32", 42)]);

// ./test/core/exports.wast:35
assert_return(() => invoke($14, `b`, []), [value("i32", 42)]);

// ./test/core/exports.wast:36
assert_return(() => invoke($14, `c`, []), [value("i32", 42)]);

// ./test/core/exports.wast:38
assert_invalid(() => instantiate(`(module (export "a" (func 0)))`), `unknown function`);

// ./test/core/exports.wast:42
assert_invalid(
  () => instantiate(`(module (func) (export "a" (func 1)))`),
  `unknown function`,
);

// ./test/core/exports.wast:46
assert_invalid(
  () => instantiate(`(module (import "spectest" "print_i32" (func (param i32))) (export "a" (func 1)))`),
  `unknown function`,
);

// ./test/core/exports.wast:50
assert_invalid(
  () => instantiate(`(module (func) (export "a" (func 0)) (export "a" (func 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:54
assert_invalid(
  () => instantiate(`(module (func) (func) (export "a" (func 0)) (export "a" (func 1)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:58
assert_invalid(
  () => instantiate(`(module (func) (global i32 (i32.const 0)) (export "a" (func 0)) (export "a" (global 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:62
assert_invalid(
  () => instantiate(`(module (func) (table 0 funcref) (export "a" (func 0)) (export "a" (table 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:66
assert_invalid(
  () => instantiate(`(module (func) (memory 0) (export "a" (func 0)) (export "a" (memory 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:70
assert_invalid(
  () => instantiate(`(module (tag $$t0 (export "t0")) (tag $$t1 (export "t0")))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:78
let $15 = instantiate(`(module (global i32 (i32.const 0)) (export "a" (global 0)))`);

// ./test/core/exports.wast:79
let $16 = instantiate(`(module (global i32 (i32.const 0)) (export "a" (global 0)) (export "b" (global 0)))`);

// ./test/core/exports.wast:80
let $17 = instantiate(`(module (global i32 (i32.const 0)) (global i32 (i32.const 0)) (export "a" (global 0)) (export "b" (global 1)))`);

// ./test/core/exports.wast:82
let $18 = instantiate(`(module (global (export "a") i32 (i32.const 0)))`);

// ./test/core/exports.wast:83
let $19 = instantiate(`(module (global i32 (i32.const 0)) (export "a" (global 0)))`);

// ./test/core/exports.wast:84
let $20 = instantiate(`(module (global $$a (export "a") i32 (i32.const 0)))`);

// ./test/core/exports.wast:85
let $21 = instantiate(`(module (global $$a i32 (i32.const 0)) (export "a" (global $$a)))`);

// ./test/core/exports.wast:86
let $22 = instantiate(`(module (export "a" (global 0)) (global i32 (i32.const 0)))`);

// ./test/core/exports.wast:87
let $23 = instantiate(`(module (export "a" (global $$a)) (global $$a i32 (i32.const 0)))`);

// ./test/core/exports.wast:89
let $24 = instantiate(`(module $$Global
  (export "e" (global $$g))
  (global $$g i32 (i32.const 42))
)`);
let $Global = $24;

// ./test/core/exports.wast:93
assert_return(() => get($24, `e`), [value("i32", 42)]);

// ./test/core/exports.wast:94
assert_return(() => get($Global, `e`), [value("i32", 42)]);

// ./test/core/exports.wast:95
let $25 = instantiate(`(module)`);

// ./test/core/exports.wast:96
let $26 = instantiate(`(module $$Other2)`);
let $Other2 = $26;

// ./test/core/exports.wast:97
assert_return(() => get($Global, `e`), [value("i32", 42)]);

// ./test/core/exports.wast:99
assert_invalid(() => instantiate(`(module (export "a" (global 0)))`), `unknown global`);

// ./test/core/exports.wast:103
assert_invalid(
  () => instantiate(`(module (global i32 (i32.const 0)) (export "a" (global 1)))`),
  `unknown global`,
);

// ./test/core/exports.wast:107
assert_invalid(
  () => instantiate(`(module (import "spectest" "global_i32" (global i32)) (export "a" (global 1)))`),
  `unknown global`,
);

// ./test/core/exports.wast:111
assert_invalid(
  () => instantiate(`(module (global i32 (i32.const 0)) (export "a" (global 0)) (export "a" (global 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:115
assert_invalid(
  () => instantiate(`(module (global i32 (i32.const 0)) (global i32 (i32.const 0)) (export "a" (global 0)) (export "a" (global 1)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:119
assert_invalid(
  () => instantiate(`(module (global i32 (i32.const 0)) (func) (export "a" (global 0)) (export "a" (func 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:123
assert_invalid(
  () => instantiate(`(module (global i32 (i32.const 0)) (table 0 funcref) (export "a" (global 0)) (export "a" (table 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:127
assert_invalid(
  () => instantiate(`(module (global i32 (i32.const 0)) (memory 0) (export "a" (global 0)) (export "a" (memory 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:135
let $27 = instantiate(`(module (table 0 funcref) (export "a" (table 0)))`);

// ./test/core/exports.wast:136
let $28 = instantiate(`(module (table 0 funcref) (export "a" (table 0)) (export "b" (table 0)))`);

// ./test/core/exports.wast:137
let $29 = instantiate(`(module (table 0 funcref) (table 0 funcref) (export "a" (table 0)) (export "b" (table 1)))`);

// ./test/core/exports.wast:139
let $30 = instantiate(`(module (table (export "a") 0 funcref))`);

// ./test/core/exports.wast:140
let $31 = instantiate(`(module (table (export "a") 0 1 funcref))`);

// ./test/core/exports.wast:141
let $32 = instantiate(`(module (table 0 funcref) (export "a" (table 0)))`);

// ./test/core/exports.wast:142
let $33 = instantiate(`(module (table 0 1 funcref) (export "a" (table 0)))`);

// ./test/core/exports.wast:143
let $34 = instantiate(`(module (table $$a (export "a") 0 funcref))`);

// ./test/core/exports.wast:144
let $35 = instantiate(`(module (table $$a (export "a") 0 1 funcref))`);

// ./test/core/exports.wast:145
let $36 = instantiate(`(module (table $$a 0 funcref) (export "a" (table $$a)))`);

// ./test/core/exports.wast:146
let $37 = instantiate(`(module (table $$a 0 1 funcref) (export "a" (table $$a)))`);

// ./test/core/exports.wast:147
let $38 = instantiate(`(module (export "a" (table 0)) (table 0 funcref))`);

// ./test/core/exports.wast:148
let $39 = instantiate(`(module (export "a" (table 0)) (table 0 1 funcref))`);

// ./test/core/exports.wast:149
let $40 = instantiate(`(module (export "a" (table $$a)) (table $$a 0 funcref))`);

// ./test/core/exports.wast:150
let $41 = instantiate(`(module (export "a" (table $$a)) (table $$a 0 1 funcref))`);

// ./test/core/exports.wast:154
assert_invalid(() => instantiate(`(module (export "a" (table 0)))`), `unknown table`);

// ./test/core/exports.wast:158
assert_invalid(
  () => instantiate(`(module (table 0 funcref) (export "a" (table 1)))`),
  `unknown table`,
);

// ./test/core/exports.wast:162
assert_invalid(
  () => instantiate(`(module  (import "spectest" "table" (table 10 20 funcref)) (export "a" (table 1)))`),
  `unknown table`,
);

// ./test/core/exports.wast:166
assert_invalid(
  () => instantiate(`(module (table 0 funcref) (export "a" (table 0)) (export "a" (table 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:170
assert_invalid(
  () => instantiate(`(module (table 0 funcref) (table 0 funcref) (export "a" (table 0)) (export "a" (table 1)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:174
assert_invalid(
  () => instantiate(`(module (table 0 funcref) (func) (export "a" (table 0)) (export "a" (func 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:178
assert_invalid(
  () => instantiate(`(module (table 0 funcref) (global i32 (i32.const 0)) (export "a" (table 0)) (export "a" (global 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:182
assert_invalid(
  () => instantiate(`(module (table 0 funcref) (memory 0) (export "a" (table 0)) (export "a" (memory 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:190
let $42 = instantiate(`(module (memory 0) (export "a" (memory 0)))`);

// ./test/core/exports.wast:191
let $43 = instantiate(`(module (memory 0) (export "a" (memory 0)) (export "b" (memory 0)))`);

// ./test/core/exports.wast:195
let $44 = instantiate(`(module (memory (export "a") 0))`);

// ./test/core/exports.wast:196
let $45 = instantiate(`(module (memory (export "a") 0 1))`);

// ./test/core/exports.wast:197
let $46 = instantiate(`(module (memory 0) (export "a" (memory 0)))`);

// ./test/core/exports.wast:198
let $47 = instantiate(`(module (memory 0 1) (export "a" (memory 0)))`);

// ./test/core/exports.wast:199
let $48 = instantiate(`(module (memory $$a (export "a") 0))`);

// ./test/core/exports.wast:200
let $49 = instantiate(`(module (memory $$a (export "a") 0 1))`);

// ./test/core/exports.wast:201
let $50 = instantiate(`(module (memory $$a 0) (export "a" (memory $$a)))`);

// ./test/core/exports.wast:202
let $51 = instantiate(`(module (memory $$a 0 1) (export "a" (memory $$a)))`);

// ./test/core/exports.wast:203
let $52 = instantiate(`(module (export "a" (memory 0)) (memory 0))`);

// ./test/core/exports.wast:204
let $53 = instantiate(`(module (export "a" (memory 0)) (memory 0 1))`);

// ./test/core/exports.wast:205
let $54 = instantiate(`(module (export "a" (memory $$a)) (memory $$a 0))`);

// ./test/core/exports.wast:206
let $55 = instantiate(`(module (export "a" (memory $$a)) (memory $$a 0 1))`);

// ./test/core/exports.wast:210
assert_invalid(() => instantiate(`(module (export "a" (memory 0)))`), `unknown memory`);

// ./test/core/exports.wast:214
assert_invalid(
  () => instantiate(`(module (memory 0) (export "a" (memory 1)))`),
  `unknown memory`,
);

// ./test/core/exports.wast:218
assert_invalid(
  () => instantiate(`(module  (import "spectest" "memory" (memory 1 2)) (export "a" (memory 1)))`),
  `unknown memory`,
);

// ./test/core/exports.wast:222
assert_invalid(
  () => instantiate(`(module (memory 0) (export "a" (memory 0)) (export "a" (memory 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:231
assert_invalid(
  () => instantiate(`(module (memory 0) (func) (export "a" (memory 0)) (export "a" (func 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:235
assert_invalid(
  () => instantiate(`(module (memory 0) (global i32 (i32.const 0)) (export "a" (memory 0)) (export "a" (global 0)))`),
  `duplicate export name`,
);

// ./test/core/exports.wast:239
assert_invalid(
  () => instantiate(`(module (memory 0) (table 0 funcref) (export "a" (memory 0)) (export "a" (table 0)))`),
  `duplicate export name`,
);
