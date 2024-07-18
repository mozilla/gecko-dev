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

// ./test/core/memory_init.wast

// ./test/core/memory_init.wast:6
let $0 = instantiate(`(module
  (memory (export "memory0") 1 1)
  (data (i32.const 2) "\\03\\01\\04\\01")
  (data "\\02\\07\\01\\08")
  (data (i32.const 12) "\\07\\05\\02\\03\\06")
  (data "\\05\\09\\02\\07\\06")
  (func (export "test")
    (nop))
  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0))))`);

// ./test/core/memory_init.wast:17
invoke($0, `test`, []);

// ./test/core/memory_init.wast:19
assert_return(() => invoke($0, `load8_u`, [0]), [value("i32", 0)]);

// ./test/core/memory_init.wast:20
assert_return(() => invoke($0, `load8_u`, [1]), [value("i32", 0)]);

// ./test/core/memory_init.wast:21
assert_return(() => invoke($0, `load8_u`, [2]), [value("i32", 3)]);

// ./test/core/memory_init.wast:22
assert_return(() => invoke($0, `load8_u`, [3]), [value("i32", 1)]);

// ./test/core/memory_init.wast:23
assert_return(() => invoke($0, `load8_u`, [4]), [value("i32", 4)]);

// ./test/core/memory_init.wast:24
assert_return(() => invoke($0, `load8_u`, [5]), [value("i32", 1)]);

// ./test/core/memory_init.wast:25
assert_return(() => invoke($0, `load8_u`, [6]), [value("i32", 0)]);

// ./test/core/memory_init.wast:26
assert_return(() => invoke($0, `load8_u`, [7]), [value("i32", 0)]);

// ./test/core/memory_init.wast:27
assert_return(() => invoke($0, `load8_u`, [8]), [value("i32", 0)]);

// ./test/core/memory_init.wast:28
assert_return(() => invoke($0, `load8_u`, [9]), [value("i32", 0)]);

// ./test/core/memory_init.wast:29
assert_return(() => invoke($0, `load8_u`, [10]), [value("i32", 0)]);

// ./test/core/memory_init.wast:30
assert_return(() => invoke($0, `load8_u`, [11]), [value("i32", 0)]);

// ./test/core/memory_init.wast:31
assert_return(() => invoke($0, `load8_u`, [12]), [value("i32", 7)]);

// ./test/core/memory_init.wast:32
assert_return(() => invoke($0, `load8_u`, [13]), [value("i32", 5)]);

// ./test/core/memory_init.wast:33
assert_return(() => invoke($0, `load8_u`, [14]), [value("i32", 2)]);

// ./test/core/memory_init.wast:34
assert_return(() => invoke($0, `load8_u`, [15]), [value("i32", 3)]);

// ./test/core/memory_init.wast:35
assert_return(() => invoke($0, `load8_u`, [16]), [value("i32", 6)]);

// ./test/core/memory_init.wast:36
assert_return(() => invoke($0, `load8_u`, [17]), [value("i32", 0)]);

// ./test/core/memory_init.wast:37
assert_return(() => invoke($0, `load8_u`, [18]), [value("i32", 0)]);

// ./test/core/memory_init.wast:38
assert_return(() => invoke($0, `load8_u`, [19]), [value("i32", 0)]);

// ./test/core/memory_init.wast:39
assert_return(() => invoke($0, `load8_u`, [20]), [value("i32", 0)]);

// ./test/core/memory_init.wast:40
assert_return(() => invoke($0, `load8_u`, [21]), [value("i32", 0)]);

// ./test/core/memory_init.wast:41
assert_return(() => invoke($0, `load8_u`, [22]), [value("i32", 0)]);

// ./test/core/memory_init.wast:42
assert_return(() => invoke($0, `load8_u`, [23]), [value("i32", 0)]);

// ./test/core/memory_init.wast:43
assert_return(() => invoke($0, `load8_u`, [24]), [value("i32", 0)]);

// ./test/core/memory_init.wast:44
assert_return(() => invoke($0, `load8_u`, [25]), [value("i32", 0)]);

// ./test/core/memory_init.wast:45
assert_return(() => invoke($0, `load8_u`, [26]), [value("i32", 0)]);

// ./test/core/memory_init.wast:46
assert_return(() => invoke($0, `load8_u`, [27]), [value("i32", 0)]);

// ./test/core/memory_init.wast:47
assert_return(() => invoke($0, `load8_u`, [28]), [value("i32", 0)]);

// ./test/core/memory_init.wast:48
assert_return(() => invoke($0, `load8_u`, [29]), [value("i32", 0)]);

// ./test/core/memory_init.wast:50
let $1 = instantiate(`(module
  (memory (export "memory0") 1 1)
  (data (i32.const 2) "\\03\\01\\04\\01")
  (data "\\02\\07\\01\\08")
  (data (i32.const 12) "\\07\\05\\02\\03\\06")
  (data "\\05\\09\\02\\07\\06")
  (func (export "test")
    (memory.init 1 (i32.const 7) (i32.const 0) (i32.const 4)))
  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0))))`);

// ./test/core/memory_init.wast:61
invoke($1, `test`, []);

// ./test/core/memory_init.wast:63
assert_return(() => invoke($1, `load8_u`, [0]), [value("i32", 0)]);

// ./test/core/memory_init.wast:64
assert_return(() => invoke($1, `load8_u`, [1]), [value("i32", 0)]);

// ./test/core/memory_init.wast:65
assert_return(() => invoke($1, `load8_u`, [2]), [value("i32", 3)]);

// ./test/core/memory_init.wast:66
assert_return(() => invoke($1, `load8_u`, [3]), [value("i32", 1)]);

// ./test/core/memory_init.wast:67
assert_return(() => invoke($1, `load8_u`, [4]), [value("i32", 4)]);

// ./test/core/memory_init.wast:68
assert_return(() => invoke($1, `load8_u`, [5]), [value("i32", 1)]);

// ./test/core/memory_init.wast:69
assert_return(() => invoke($1, `load8_u`, [6]), [value("i32", 0)]);

// ./test/core/memory_init.wast:70
assert_return(() => invoke($1, `load8_u`, [7]), [value("i32", 2)]);

// ./test/core/memory_init.wast:71
assert_return(() => invoke($1, `load8_u`, [8]), [value("i32", 7)]);

// ./test/core/memory_init.wast:72
assert_return(() => invoke($1, `load8_u`, [9]), [value("i32", 1)]);

// ./test/core/memory_init.wast:73
assert_return(() => invoke($1, `load8_u`, [10]), [value("i32", 8)]);

// ./test/core/memory_init.wast:74
assert_return(() => invoke($1, `load8_u`, [11]), [value("i32", 0)]);

// ./test/core/memory_init.wast:75
assert_return(() => invoke($1, `load8_u`, [12]), [value("i32", 7)]);

// ./test/core/memory_init.wast:76
assert_return(() => invoke($1, `load8_u`, [13]), [value("i32", 5)]);

// ./test/core/memory_init.wast:77
assert_return(() => invoke($1, `load8_u`, [14]), [value("i32", 2)]);

// ./test/core/memory_init.wast:78
assert_return(() => invoke($1, `load8_u`, [15]), [value("i32", 3)]);

// ./test/core/memory_init.wast:79
assert_return(() => invoke($1, `load8_u`, [16]), [value("i32", 6)]);

// ./test/core/memory_init.wast:80
assert_return(() => invoke($1, `load8_u`, [17]), [value("i32", 0)]);

// ./test/core/memory_init.wast:81
assert_return(() => invoke($1, `load8_u`, [18]), [value("i32", 0)]);

// ./test/core/memory_init.wast:82
assert_return(() => invoke($1, `load8_u`, [19]), [value("i32", 0)]);

// ./test/core/memory_init.wast:83
assert_return(() => invoke($1, `load8_u`, [20]), [value("i32", 0)]);

// ./test/core/memory_init.wast:84
assert_return(() => invoke($1, `load8_u`, [21]), [value("i32", 0)]);

// ./test/core/memory_init.wast:85
assert_return(() => invoke($1, `load8_u`, [22]), [value("i32", 0)]);

// ./test/core/memory_init.wast:86
assert_return(() => invoke($1, `load8_u`, [23]), [value("i32", 0)]);

// ./test/core/memory_init.wast:87
assert_return(() => invoke($1, `load8_u`, [24]), [value("i32", 0)]);

// ./test/core/memory_init.wast:88
assert_return(() => invoke($1, `load8_u`, [25]), [value("i32", 0)]);

// ./test/core/memory_init.wast:89
assert_return(() => invoke($1, `load8_u`, [26]), [value("i32", 0)]);

// ./test/core/memory_init.wast:90
assert_return(() => invoke($1, `load8_u`, [27]), [value("i32", 0)]);

// ./test/core/memory_init.wast:91
assert_return(() => invoke($1, `load8_u`, [28]), [value("i32", 0)]);

// ./test/core/memory_init.wast:92
assert_return(() => invoke($1, `load8_u`, [29]), [value("i32", 0)]);

// ./test/core/memory_init.wast:94
let $2 = instantiate(`(module
  (memory (export "memory0") 1 1)
  (data (i32.const 2) "\\03\\01\\04\\01")
  (data "\\02\\07\\01\\08")
  (data (i32.const 12) "\\07\\05\\02\\03\\06")
  (data "\\05\\09\\02\\07\\06")
  (func (export "test")
    (memory.init 3 (i32.const 15) (i32.const 1) (i32.const 3)))
  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0))))`);

// ./test/core/memory_init.wast:105
invoke($2, `test`, []);

// ./test/core/memory_init.wast:107
assert_return(() => invoke($2, `load8_u`, [0]), [value("i32", 0)]);

// ./test/core/memory_init.wast:108
assert_return(() => invoke($2, `load8_u`, [1]), [value("i32", 0)]);

// ./test/core/memory_init.wast:109
assert_return(() => invoke($2, `load8_u`, [2]), [value("i32", 3)]);

// ./test/core/memory_init.wast:110
assert_return(() => invoke($2, `load8_u`, [3]), [value("i32", 1)]);

// ./test/core/memory_init.wast:111
assert_return(() => invoke($2, `load8_u`, [4]), [value("i32", 4)]);

// ./test/core/memory_init.wast:112
assert_return(() => invoke($2, `load8_u`, [5]), [value("i32", 1)]);

// ./test/core/memory_init.wast:113
assert_return(() => invoke($2, `load8_u`, [6]), [value("i32", 0)]);

// ./test/core/memory_init.wast:114
assert_return(() => invoke($2, `load8_u`, [7]), [value("i32", 0)]);

// ./test/core/memory_init.wast:115
assert_return(() => invoke($2, `load8_u`, [8]), [value("i32", 0)]);

// ./test/core/memory_init.wast:116
assert_return(() => invoke($2, `load8_u`, [9]), [value("i32", 0)]);

// ./test/core/memory_init.wast:117
assert_return(() => invoke($2, `load8_u`, [10]), [value("i32", 0)]);

// ./test/core/memory_init.wast:118
assert_return(() => invoke($2, `load8_u`, [11]), [value("i32", 0)]);

// ./test/core/memory_init.wast:119
assert_return(() => invoke($2, `load8_u`, [12]), [value("i32", 7)]);

// ./test/core/memory_init.wast:120
assert_return(() => invoke($2, `load8_u`, [13]), [value("i32", 5)]);

// ./test/core/memory_init.wast:121
assert_return(() => invoke($2, `load8_u`, [14]), [value("i32", 2)]);

// ./test/core/memory_init.wast:122
assert_return(() => invoke($2, `load8_u`, [15]), [value("i32", 9)]);

// ./test/core/memory_init.wast:123
assert_return(() => invoke($2, `load8_u`, [16]), [value("i32", 2)]);

// ./test/core/memory_init.wast:124
assert_return(() => invoke($2, `load8_u`, [17]), [value("i32", 7)]);

// ./test/core/memory_init.wast:125
assert_return(() => invoke($2, `load8_u`, [18]), [value("i32", 0)]);

// ./test/core/memory_init.wast:126
assert_return(() => invoke($2, `load8_u`, [19]), [value("i32", 0)]);

// ./test/core/memory_init.wast:127
assert_return(() => invoke($2, `load8_u`, [20]), [value("i32", 0)]);

// ./test/core/memory_init.wast:128
assert_return(() => invoke($2, `load8_u`, [21]), [value("i32", 0)]);

// ./test/core/memory_init.wast:129
assert_return(() => invoke($2, `load8_u`, [22]), [value("i32", 0)]);

// ./test/core/memory_init.wast:130
assert_return(() => invoke($2, `load8_u`, [23]), [value("i32", 0)]);

// ./test/core/memory_init.wast:131
assert_return(() => invoke($2, `load8_u`, [24]), [value("i32", 0)]);

// ./test/core/memory_init.wast:132
assert_return(() => invoke($2, `load8_u`, [25]), [value("i32", 0)]);

// ./test/core/memory_init.wast:133
assert_return(() => invoke($2, `load8_u`, [26]), [value("i32", 0)]);

// ./test/core/memory_init.wast:134
assert_return(() => invoke($2, `load8_u`, [27]), [value("i32", 0)]);

// ./test/core/memory_init.wast:135
assert_return(() => invoke($2, `load8_u`, [28]), [value("i32", 0)]);

// ./test/core/memory_init.wast:136
assert_return(() => invoke($2, `load8_u`, [29]), [value("i32", 0)]);

// ./test/core/memory_init.wast:138
let $3 = instantiate(`(module
  (memory (export "memory0") 1 1)
  (data (i32.const 2) "\\03\\01\\04\\01")
  (data "\\02\\07\\01\\08")
  (data (i32.const 12) "\\07\\05\\02\\03\\06")
  (data "\\05\\09\\02\\07\\06")
  (func (export "test")
    (memory.init 1 (i32.const 7) (i32.const 0) (i32.const 4))
    (data.drop 1)
    (memory.init 3 (i32.const 15) (i32.const 1) (i32.const 3))
    (data.drop 3)
    (memory.copy (i32.const 20) (i32.const 15) (i32.const 5))
    (memory.copy (i32.const 21) (i32.const 29) (i32.const 1))
    (memory.copy (i32.const 24) (i32.const 10) (i32.const 1))
    (memory.copy (i32.const 13) (i32.const 11) (i32.const 4))
    (memory.copy (i32.const 19) (i32.const 20) (i32.const 5)))
  (func (export "load8_u") (param i32) (result i32)
    (i32.load8_u (local.get 0))))`);

// ./test/core/memory_init.wast:157
invoke($3, `test`, []);

// ./test/core/memory_init.wast:159
assert_return(() => invoke($3, `load8_u`, [0]), [value("i32", 0)]);

// ./test/core/memory_init.wast:160
assert_return(() => invoke($3, `load8_u`, [1]), [value("i32", 0)]);

// ./test/core/memory_init.wast:161
assert_return(() => invoke($3, `load8_u`, [2]), [value("i32", 3)]);

// ./test/core/memory_init.wast:162
assert_return(() => invoke($3, `load8_u`, [3]), [value("i32", 1)]);

// ./test/core/memory_init.wast:163
assert_return(() => invoke($3, `load8_u`, [4]), [value("i32", 4)]);

// ./test/core/memory_init.wast:164
assert_return(() => invoke($3, `load8_u`, [5]), [value("i32", 1)]);

// ./test/core/memory_init.wast:165
assert_return(() => invoke($3, `load8_u`, [6]), [value("i32", 0)]);

// ./test/core/memory_init.wast:166
assert_return(() => invoke($3, `load8_u`, [7]), [value("i32", 2)]);

// ./test/core/memory_init.wast:167
assert_return(() => invoke($3, `load8_u`, [8]), [value("i32", 7)]);

// ./test/core/memory_init.wast:168
assert_return(() => invoke($3, `load8_u`, [9]), [value("i32", 1)]);

// ./test/core/memory_init.wast:169
assert_return(() => invoke($3, `load8_u`, [10]), [value("i32", 8)]);

// ./test/core/memory_init.wast:170
assert_return(() => invoke($3, `load8_u`, [11]), [value("i32", 0)]);

// ./test/core/memory_init.wast:171
assert_return(() => invoke($3, `load8_u`, [12]), [value("i32", 7)]);

// ./test/core/memory_init.wast:172
assert_return(() => invoke($3, `load8_u`, [13]), [value("i32", 0)]);

// ./test/core/memory_init.wast:173
assert_return(() => invoke($3, `load8_u`, [14]), [value("i32", 7)]);

// ./test/core/memory_init.wast:174
assert_return(() => invoke($3, `load8_u`, [15]), [value("i32", 5)]);

// ./test/core/memory_init.wast:175
assert_return(() => invoke($3, `load8_u`, [16]), [value("i32", 2)]);

// ./test/core/memory_init.wast:176
assert_return(() => invoke($3, `load8_u`, [17]), [value("i32", 7)]);

// ./test/core/memory_init.wast:177
assert_return(() => invoke($3, `load8_u`, [18]), [value("i32", 0)]);

// ./test/core/memory_init.wast:178
assert_return(() => invoke($3, `load8_u`, [19]), [value("i32", 9)]);

// ./test/core/memory_init.wast:179
assert_return(() => invoke($3, `load8_u`, [20]), [value("i32", 0)]);

// ./test/core/memory_init.wast:180
assert_return(() => invoke($3, `load8_u`, [21]), [value("i32", 7)]);

// ./test/core/memory_init.wast:181
assert_return(() => invoke($3, `load8_u`, [22]), [value("i32", 0)]);

// ./test/core/memory_init.wast:182
assert_return(() => invoke($3, `load8_u`, [23]), [value("i32", 8)]);

// ./test/core/memory_init.wast:183
assert_return(() => invoke($3, `load8_u`, [24]), [value("i32", 8)]);

// ./test/core/memory_init.wast:184
assert_return(() => invoke($3, `load8_u`, [25]), [value("i32", 0)]);

// ./test/core/memory_init.wast:185
assert_return(() => invoke($3, `load8_u`, [26]), [value("i32", 0)]);

// ./test/core/memory_init.wast:186
assert_return(() => invoke($3, `load8_u`, [27]), [value("i32", 0)]);

// ./test/core/memory_init.wast:187
assert_return(() => invoke($3, `load8_u`, [28]), [value("i32", 0)]);

// ./test/core/memory_init.wast:188
assert_return(() => invoke($3, `load8_u`, [29]), [value("i32", 0)]);

// ./test/core/memory_init.wast:189
assert_invalid(
  () => instantiate(`(module
     (func (export "test")
       (data.drop 0)))`),
  `unknown data segment`,
);

// ./test/core/memory_init.wast:195
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (data.drop 4)))`),
  `unknown data segment`,
);

// ./test/core/memory_init.wast:203
let $4 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (data.drop 0)
    (data.drop 0)))`);

// ./test/core/memory_init.wast:209
invoke($4, `test`, []);

// ./test/core/memory_init.wast:211
let $5 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (data.drop 0)
    (memory.init 0 (i32.const 1234) (i32.const 1) (i32.const 1))))`);

// ./test/core/memory_init.wast:217
assert_trap(() => invoke($5, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:219
let $6 = instantiate(`(module
   (memory 1)
   (data (i32.const 0) "\\37")
   (func (export "test")
     (memory.init 0 (i32.const 1234) (i32.const 1) (i32.const 1))))`);

// ./test/core/memory_init.wast:224
assert_trap(() => invoke($6, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:226
assert_invalid(
  () => instantiate(`(module
    (func (export "test")
      (memory.init 1 (i32.const 1234) (i32.const 1) (i32.const 1))))`),
  `unknown memory 0`,
);

// ./test/core/memory_init.wast:232
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 1 (i32.const 1234) (i32.const 1) (i32.const 1))))`),
  `unknown data segment 1`,
);

// ./test/core/memory_init.wast:240
let $7 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 1) (i32.const 0) (i32.const 1))
    (memory.init 0 (i32.const 1) (i32.const 0) (i32.const 1))))`);

// ./test/core/memory_init.wast:246
invoke($7, `test`, []);

// ./test/core/memory_init.wast:248
let $8 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 1234) (i32.const 0) (i32.const 5))))`);

// ./test/core/memory_init.wast:253
assert_trap(() => invoke($8, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:255
let $9 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 1234) (i32.const 2) (i32.const 3))))`);

// ./test/core/memory_init.wast:260
assert_trap(() => invoke($9, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:262
let $10 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 0xFFFE) (i32.const 1) (i32.const 3))))`);

// ./test/core/memory_init.wast:267
assert_trap(() => invoke($10, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:269
let $11 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 1234) (i32.const 4) (i32.const 0))))`);

// ./test/core/memory_init.wast:274
assert_trap(() => invoke($11, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:276
let $12 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 1234) (i32.const 1) (i32.const 0))))`);

// ./test/core/memory_init.wast:281
invoke($12, `test`, []);

// ./test/core/memory_init.wast:283
let $13 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 0x10001) (i32.const 0) (i32.const 0))))`);

// ./test/core/memory_init.wast:288
assert_trap(() => invoke($13, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:290
let $14 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 0x10000) (i32.const 0) (i32.const 0))))`);

// ./test/core/memory_init.wast:295
invoke($14, `test`, []);

// ./test/core/memory_init.wast:297
let $15 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 0x10000) (i32.const 1) (i32.const 0))))`);

// ./test/core/memory_init.wast:302
invoke($15, `test`, []);

// ./test/core/memory_init.wast:304
let $16 = instantiate(`(module
  (memory 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i32.const 0x10001) (i32.const 4) (i32.const 0))))`);

// ./test/core/memory_init.wast:309
assert_trap(() => invoke($16, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:311
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:319
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:327
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:335
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:343
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:351
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:359
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:367
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:375
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:383
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:391
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:399
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:407
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:415
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:423
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:431
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:439
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:447
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:455
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:463
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:471
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:479
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:487
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:495
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:503
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:511
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:519
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:527
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:535
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:543
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:551
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:559
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:567
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:575
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:583
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:591
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:599
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:607
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:615
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:623
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:631
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:639
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:647
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:655
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:663
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:671
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:679
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:687
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:695
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:703
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:711
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:719
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:727
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:735
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:743
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:751
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:759
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:767
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:775
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:783
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:791
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:799
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:807
assert_invalid(
  () => instantiate(`(module
    (memory 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:815
let $17 = instantiate(`(module
  (memory 1 1 )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:833
assert_trap(() => invoke($17, `run`, [65528, 16]), `out of bounds memory access`);

// ./test/core/memory_init.wast:836
assert_return(() => invoke($17, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_init.wast:838
let $18 = instantiate(`(module
  (memory 1 1 )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:856
assert_trap(() => invoke($18, `run`, [65527, 16]), `out of bounds memory access`);

// ./test/core/memory_init.wast:859
assert_return(() => invoke($18, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_init.wast:861
let $19 = instantiate(`(module
  (memory 1 1 )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:879
assert_trap(() => invoke($19, `run`, [65472, 30]), `out of bounds memory access`);

// ./test/core/memory_init.wast:882
assert_return(() => invoke($19, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_init.wast:884
let $20 = instantiate(`(module
  (memory 1 1 )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:902
assert_trap(() => invoke($20, `run`, [65473, 31]), `out of bounds memory access`);

// ./test/core/memory_init.wast:905
assert_return(() => invoke($20, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_init.wast:907
let $21 = instantiate(`(module
  (memory 1  )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:925
assert_trap(() => invoke($21, `run`, [65528, -256]), `out of bounds memory access`);

// ./test/core/memory_init.wast:928
assert_return(() => invoke($21, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_init.wast:930
let $22 = instantiate(`(module
  (memory 1  )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i32) (param $$to i32) (param $$expected i32) (result i32)
    (loop $$cont
      (if (i32.eq (local.get $$from) (local.get $$to))
        (then
          (return (i32.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i32.add (local.get $$from) (i32.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i32) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:948
assert_trap(() => invoke($22, `run`, [0, -4]), `out of bounds memory access`);

// ./test/core/memory_init.wast:951
assert_return(() => invoke($22, `checkRange`, [0, 1, 0]), [value("i32", -1)]);

// ./test/core/memory_init.wast:954
let $23 = instantiate(`(module
  (memory 1)
  ;; 65 data segments. 64 is the smallest positive number that is encoded
  ;; differently as a signed LEB.
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "")
  (func (memory.init 64 (i32.const 0) (i32.const 0) (i32.const 0))))`);

// ./test/core/memory_init.wast:969
let $24 = instantiate(`(module
  (memory (export "memory0") i64 1 1)
  (data (i64.const 2) "\\03\\01\\04\\01")
  (data "\\02\\07\\01\\08")
  (data (i64.const 12) "\\07\\05\\02\\03\\06")
  (data "\\05\\09\\02\\07\\06")
  (func (export "test")
    (nop))
  (func (export "load8_u") (param i64) (result i32)
    (i32.load8_u (local.get 0))))`);

// ./test/core/memory_init.wast:980
invoke($24, `test`, []);

// ./test/core/memory_init.wast:982
assert_return(() => invoke($24, `load8_u`, [0n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:983
assert_return(() => invoke($24, `load8_u`, [1n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:984
assert_return(() => invoke($24, `load8_u`, [2n]), [value("i32", 3)]);

// ./test/core/memory_init.wast:985
assert_return(() => invoke($24, `load8_u`, [3n]), [value("i32", 1)]);

// ./test/core/memory_init.wast:986
assert_return(() => invoke($24, `load8_u`, [4n]), [value("i32", 4)]);

// ./test/core/memory_init.wast:987
assert_return(() => invoke($24, `load8_u`, [5n]), [value("i32", 1)]);

// ./test/core/memory_init.wast:988
assert_return(() => invoke($24, `load8_u`, [6n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:989
assert_return(() => invoke($24, `load8_u`, [7n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:990
assert_return(() => invoke($24, `load8_u`, [8n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:991
assert_return(() => invoke($24, `load8_u`, [9n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:992
assert_return(() => invoke($24, `load8_u`, [10n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:993
assert_return(() => invoke($24, `load8_u`, [11n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:994
assert_return(() => invoke($24, `load8_u`, [12n]), [value("i32", 7)]);

// ./test/core/memory_init.wast:995
assert_return(() => invoke($24, `load8_u`, [13n]), [value("i32", 5)]);

// ./test/core/memory_init.wast:996
assert_return(() => invoke($24, `load8_u`, [14n]), [value("i32", 2)]);

// ./test/core/memory_init.wast:997
assert_return(() => invoke($24, `load8_u`, [15n]), [value("i32", 3)]);

// ./test/core/memory_init.wast:998
assert_return(() => invoke($24, `load8_u`, [16n]), [value("i32", 6)]);

// ./test/core/memory_init.wast:999
assert_return(() => invoke($24, `load8_u`, [17n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1000
assert_return(() => invoke($24, `load8_u`, [18n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1001
assert_return(() => invoke($24, `load8_u`, [19n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1002
assert_return(() => invoke($24, `load8_u`, [20n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1003
assert_return(() => invoke($24, `load8_u`, [21n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1004
assert_return(() => invoke($24, `load8_u`, [22n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1005
assert_return(() => invoke($24, `load8_u`, [23n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1006
assert_return(() => invoke($24, `load8_u`, [24n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1007
assert_return(() => invoke($24, `load8_u`, [25n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1008
assert_return(() => invoke($24, `load8_u`, [26n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1009
assert_return(() => invoke($24, `load8_u`, [27n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1010
assert_return(() => invoke($24, `load8_u`, [28n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1011
assert_return(() => invoke($24, `load8_u`, [29n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1013
let $25 = instantiate(`(module
  (memory (export "memory0") i64 1 1)
  (data (i64.const 2) "\\03\\01\\04\\01")
  (data "\\02\\07\\01\\08")
  (data (i64.const 12) "\\07\\05\\02\\03\\06")
  (data "\\05\\09\\02\\07\\06")
  (func (export "test")
    (memory.init 1 (i64.const 7) (i32.const 0) (i32.const 4)))
  (func (export "load8_u") (param i64) (result i32)
    (i32.load8_u (local.get 0))))`);

// ./test/core/memory_init.wast:1024
invoke($25, `test`, []);

// ./test/core/memory_init.wast:1026
assert_return(() => invoke($25, `load8_u`, [0n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1027
assert_return(() => invoke($25, `load8_u`, [1n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1028
assert_return(() => invoke($25, `load8_u`, [2n]), [value("i32", 3)]);

// ./test/core/memory_init.wast:1029
assert_return(() => invoke($25, `load8_u`, [3n]), [value("i32", 1)]);

// ./test/core/memory_init.wast:1030
assert_return(() => invoke($25, `load8_u`, [4n]), [value("i32", 4)]);

// ./test/core/memory_init.wast:1031
assert_return(() => invoke($25, `load8_u`, [5n]), [value("i32", 1)]);

// ./test/core/memory_init.wast:1032
assert_return(() => invoke($25, `load8_u`, [6n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1033
assert_return(() => invoke($25, `load8_u`, [7n]), [value("i32", 2)]);

// ./test/core/memory_init.wast:1034
assert_return(() => invoke($25, `load8_u`, [8n]), [value("i32", 7)]);

// ./test/core/memory_init.wast:1035
assert_return(() => invoke($25, `load8_u`, [9n]), [value("i32", 1)]);

// ./test/core/memory_init.wast:1036
assert_return(() => invoke($25, `load8_u`, [10n]), [value("i32", 8)]);

// ./test/core/memory_init.wast:1037
assert_return(() => invoke($25, `load8_u`, [11n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1038
assert_return(() => invoke($25, `load8_u`, [12n]), [value("i32", 7)]);

// ./test/core/memory_init.wast:1039
assert_return(() => invoke($25, `load8_u`, [13n]), [value("i32", 5)]);

// ./test/core/memory_init.wast:1040
assert_return(() => invoke($25, `load8_u`, [14n]), [value("i32", 2)]);

// ./test/core/memory_init.wast:1041
assert_return(() => invoke($25, `load8_u`, [15n]), [value("i32", 3)]);

// ./test/core/memory_init.wast:1042
assert_return(() => invoke($25, `load8_u`, [16n]), [value("i32", 6)]);

// ./test/core/memory_init.wast:1043
assert_return(() => invoke($25, `load8_u`, [17n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1044
assert_return(() => invoke($25, `load8_u`, [18n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1045
assert_return(() => invoke($25, `load8_u`, [19n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1046
assert_return(() => invoke($25, `load8_u`, [20n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1047
assert_return(() => invoke($25, `load8_u`, [21n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1048
assert_return(() => invoke($25, `load8_u`, [22n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1049
assert_return(() => invoke($25, `load8_u`, [23n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1050
assert_return(() => invoke($25, `load8_u`, [24n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1051
assert_return(() => invoke($25, `load8_u`, [25n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1052
assert_return(() => invoke($25, `load8_u`, [26n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1053
assert_return(() => invoke($25, `load8_u`, [27n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1054
assert_return(() => invoke($25, `load8_u`, [28n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1055
assert_return(() => invoke($25, `load8_u`, [29n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1057
let $26 = instantiate(`(module
  (memory (export "memory0") i64 1 1)
  (data (i64.const 2) "\\03\\01\\04\\01")
  (data "\\02\\07\\01\\08")
  (data (i64.const 12) "\\07\\05\\02\\03\\06")
  (data "\\05\\09\\02\\07\\06")
  (func (export "test")
    (memory.init 3 (i64.const 15) (i32.const 1) (i32.const 3)))
  (func (export "load8_u") (param i64) (result i32)
    (i32.load8_u (local.get 0))))`);

// ./test/core/memory_init.wast:1068
invoke($26, `test`, []);

// ./test/core/memory_init.wast:1070
assert_return(() => invoke($26, `load8_u`, [0n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1071
assert_return(() => invoke($26, `load8_u`, [1n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1072
assert_return(() => invoke($26, `load8_u`, [2n]), [value("i32", 3)]);

// ./test/core/memory_init.wast:1073
assert_return(() => invoke($26, `load8_u`, [3n]), [value("i32", 1)]);

// ./test/core/memory_init.wast:1074
assert_return(() => invoke($26, `load8_u`, [4n]), [value("i32", 4)]);

// ./test/core/memory_init.wast:1075
assert_return(() => invoke($26, `load8_u`, [5n]), [value("i32", 1)]);

// ./test/core/memory_init.wast:1076
assert_return(() => invoke($26, `load8_u`, [6n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1077
assert_return(() => invoke($26, `load8_u`, [7n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1078
assert_return(() => invoke($26, `load8_u`, [8n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1079
assert_return(() => invoke($26, `load8_u`, [9n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1080
assert_return(() => invoke($26, `load8_u`, [10n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1081
assert_return(() => invoke($26, `load8_u`, [11n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1082
assert_return(() => invoke($26, `load8_u`, [12n]), [value("i32", 7)]);

// ./test/core/memory_init.wast:1083
assert_return(() => invoke($26, `load8_u`, [13n]), [value("i32", 5)]);

// ./test/core/memory_init.wast:1084
assert_return(() => invoke($26, `load8_u`, [14n]), [value("i32", 2)]);

// ./test/core/memory_init.wast:1085
assert_return(() => invoke($26, `load8_u`, [15n]), [value("i32", 9)]);

// ./test/core/memory_init.wast:1086
assert_return(() => invoke($26, `load8_u`, [16n]), [value("i32", 2)]);

// ./test/core/memory_init.wast:1087
assert_return(() => invoke($26, `load8_u`, [17n]), [value("i32", 7)]);

// ./test/core/memory_init.wast:1088
assert_return(() => invoke($26, `load8_u`, [18n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1089
assert_return(() => invoke($26, `load8_u`, [19n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1090
assert_return(() => invoke($26, `load8_u`, [20n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1091
assert_return(() => invoke($26, `load8_u`, [21n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1092
assert_return(() => invoke($26, `load8_u`, [22n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1093
assert_return(() => invoke($26, `load8_u`, [23n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1094
assert_return(() => invoke($26, `load8_u`, [24n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1095
assert_return(() => invoke($26, `load8_u`, [25n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1096
assert_return(() => invoke($26, `load8_u`, [26n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1097
assert_return(() => invoke($26, `load8_u`, [27n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1098
assert_return(() => invoke($26, `load8_u`, [28n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1099
assert_return(() => invoke($26, `load8_u`, [29n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1101
let $27 = instantiate(`(module
  (memory (export "memory0") i64 1 1)
  (data (i64.const 2) "\\03\\01\\04\\01")
  (data "\\02\\07\\01\\08")
  (data (i64.const 12) "\\07\\05\\02\\03\\06")
  (data "\\05\\09\\02\\07\\06")
  (func (export "test")
    (memory.init 1 (i64.const 7) (i32.const 0) (i32.const 4))
    (data.drop 1)
    (memory.init 3 (i64.const 15) (i32.const 1) (i32.const 3))
    (data.drop 3)
    (memory.copy (i64.const 20) (i64.const 15) (i64.const 5))
    (memory.copy (i64.const 21) (i64.const 29) (i64.const 1))
    (memory.copy (i64.const 24) (i64.const 10) (i64.const 1))
    (memory.copy (i64.const 13) (i64.const 11) (i64.const 4))
    (memory.copy (i64.const 19) (i64.const 20) (i64.const 5)))
  (func (export "load8_u") (param i64) (result i32)
    (i32.load8_u (local.get 0))))`);

// ./test/core/memory_init.wast:1120
invoke($27, `test`, []);

// ./test/core/memory_init.wast:1122
assert_return(() => invoke($27, `load8_u`, [0n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1123
assert_return(() => invoke($27, `load8_u`, [1n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1124
assert_return(() => invoke($27, `load8_u`, [2n]), [value("i32", 3)]);

// ./test/core/memory_init.wast:1125
assert_return(() => invoke($27, `load8_u`, [3n]), [value("i32", 1)]);

// ./test/core/memory_init.wast:1126
assert_return(() => invoke($27, `load8_u`, [4n]), [value("i32", 4)]);

// ./test/core/memory_init.wast:1127
assert_return(() => invoke($27, `load8_u`, [5n]), [value("i32", 1)]);

// ./test/core/memory_init.wast:1128
assert_return(() => invoke($27, `load8_u`, [6n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1129
assert_return(() => invoke($27, `load8_u`, [7n]), [value("i32", 2)]);

// ./test/core/memory_init.wast:1130
assert_return(() => invoke($27, `load8_u`, [8n]), [value("i32", 7)]);

// ./test/core/memory_init.wast:1131
assert_return(() => invoke($27, `load8_u`, [9n]), [value("i32", 1)]);

// ./test/core/memory_init.wast:1132
assert_return(() => invoke($27, `load8_u`, [10n]), [value("i32", 8)]);

// ./test/core/memory_init.wast:1133
assert_return(() => invoke($27, `load8_u`, [11n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1134
assert_return(() => invoke($27, `load8_u`, [12n]), [value("i32", 7)]);

// ./test/core/memory_init.wast:1135
assert_return(() => invoke($27, `load8_u`, [13n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1136
assert_return(() => invoke($27, `load8_u`, [14n]), [value("i32", 7)]);

// ./test/core/memory_init.wast:1137
assert_return(() => invoke($27, `load8_u`, [15n]), [value("i32", 5)]);

// ./test/core/memory_init.wast:1138
assert_return(() => invoke($27, `load8_u`, [16n]), [value("i32", 2)]);

// ./test/core/memory_init.wast:1139
assert_return(() => invoke($27, `load8_u`, [17n]), [value("i32", 7)]);

// ./test/core/memory_init.wast:1140
assert_return(() => invoke($27, `load8_u`, [18n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1141
assert_return(() => invoke($27, `load8_u`, [19n]), [value("i32", 9)]);

// ./test/core/memory_init.wast:1142
assert_return(() => invoke($27, `load8_u`, [20n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1143
assert_return(() => invoke($27, `load8_u`, [21n]), [value("i32", 7)]);

// ./test/core/memory_init.wast:1144
assert_return(() => invoke($27, `load8_u`, [22n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1145
assert_return(() => invoke($27, `load8_u`, [23n]), [value("i32", 8)]);

// ./test/core/memory_init.wast:1146
assert_return(() => invoke($27, `load8_u`, [24n]), [value("i32", 8)]);

// ./test/core/memory_init.wast:1147
assert_return(() => invoke($27, `load8_u`, [25n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1148
assert_return(() => invoke($27, `load8_u`, [26n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1149
assert_return(() => invoke($27, `load8_u`, [27n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1150
assert_return(() => invoke($27, `load8_u`, [28n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1151
assert_return(() => invoke($27, `load8_u`, [29n]), [value("i32", 0)]);

// ./test/core/memory_init.wast:1152
assert_invalid(
  () => instantiate(`(module
     (func (export "test")
       (data.drop 0)))`),
  `unknown data segment`,
);

// ./test/core/memory_init.wast:1158
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (data.drop 4)))`),
  `unknown data segment`,
);

// ./test/core/memory_init.wast:1166
let $28 = instantiate(`(module
  (memory i64 1)
    (data "\\37")
  (func (export "test")
    (data.drop 0)
    (data.drop 0)))`);

// ./test/core/memory_init.wast:1172
invoke($28, `test`, []);

// ./test/core/memory_init.wast:1174
let $29 = instantiate(`(module
  (memory i64 1)
    (data "\\37")
  (func (export "test")
    (data.drop 0)
    (memory.init 0 (i64.const 1234) (i32.const 1) (i32.const 1))))`);

// ./test/core/memory_init.wast:1180
assert_trap(() => invoke($29, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:1182
let $30 = instantiate(`(module
   (memory i64 1)
   (data (i64.const 0) "\\37")
   (func (export "test")
     (memory.init 0 (i64.const 1234) (i32.const 1) (i32.const 1))))`);

// ./test/core/memory_init.wast:1187
assert_trap(() => invoke($30, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:1189
assert_invalid(
  () => instantiate(`(module
    (func (export "test")
      (memory.init 1 (i64.const 1234) (i32.const 1) (i32.const 1))))`),
  `unknown memory 0`,
);

// ./test/core/memory_init.wast:1195
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 1 (i64.const 1234) (i32.const 1) (i32.const 1))))`),
  `unknown data segment 1`,
);

// ./test/core/memory_init.wast:1203
let $31 = instantiate(`(module
  (memory i64 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i64.const 1) (i32.const 0) (i32.const 1))
    (memory.init 0 (i64.const 1) (i32.const 0) (i32.const 1))))`);

// ./test/core/memory_init.wast:1209
invoke($31, `test`, []);

// ./test/core/memory_init.wast:1211
let $32 = instantiate(`(module
  (memory i64 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i64.const 1234) (i32.const 0) (i32.const 5))))`);

// ./test/core/memory_init.wast:1216
assert_trap(() => invoke($32, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:1218
let $33 = instantiate(`(module
  (memory i64 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i64.const 1234) (i32.const 2) (i32.const 3))))`);

// ./test/core/memory_init.wast:1223
assert_trap(() => invoke($33, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:1225
let $34 = instantiate(`(module
  (memory i64 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i64.const 0xFFFE) (i32.const 1) (i32.const 3))))`);

// ./test/core/memory_init.wast:1230
assert_trap(() => invoke($34, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:1232
let $35 = instantiate(`(module
  (memory i64 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i64.const 1234) (i32.const 4) (i32.const 0))))`);

// ./test/core/memory_init.wast:1237
assert_trap(() => invoke($35, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:1239
let $36 = instantiate(`(module
  (memory i64 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i64.const 1234) (i32.const 1) (i32.const 0))))`);

// ./test/core/memory_init.wast:1244
invoke($36, `test`, []);

// ./test/core/memory_init.wast:1246
let $37 = instantiate(`(module
  (memory i64 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i64.const 0x10001) (i32.const 0) (i32.const 0))))`);

// ./test/core/memory_init.wast:1251
assert_trap(() => invoke($37, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:1253
let $38 = instantiate(`(module
  (memory i64 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i64.const 0x10000) (i32.const 0) (i32.const 0))))`);

// ./test/core/memory_init.wast:1258
invoke($38, `test`, []);

// ./test/core/memory_init.wast:1260
let $39 = instantiate(`(module
  (memory i64 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i64.const 0x10000) (i32.const 1) (i32.const 0))))`);

// ./test/core/memory_init.wast:1265
invoke($39, `test`, []);

// ./test/core/memory_init.wast:1267
let $40 = instantiate(`(module
  (memory i64 1)
    (data "\\37")
  (func (export "test")
    (memory.init 0 (i64.const 0x10001) (i32.const 4) (i32.const 0))))`);

// ./test/core/memory_init.wast:1272
assert_trap(() => invoke($40, `test`, []), `out of bounds memory access`);

// ./test/core/memory_init.wast:1274
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1282
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1290
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1298
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1306
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1314
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1322
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1330
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1338
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1346
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1354
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1362
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1370
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1378
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1386
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1394
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i32.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1402
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1410
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1418
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1426
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1434
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1442
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1450
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1458
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1466
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1474
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1482
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1490
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1498
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1506
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1514
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1522
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f32.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1530
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1538
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1546
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1554
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1562
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1570
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1578
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1586
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1594
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1602
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1610
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1618
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1626
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1634
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1642
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (i64.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1650
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1658
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1666
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1674
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1682
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f32.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1690
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f32.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1698
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f32.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1706
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f32.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1714
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1722
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1730
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1738
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (i64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1746
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1754
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f64.const 1) (f32.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1762
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f64.const 1) (i64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1770
assert_invalid(
  () => instantiate(`(module
    (memory i64 1)
    (data "\\37")
    (func (export "test")
      (memory.init 0 (f64.const 1) (f64.const 1) (f64.const 1))))`),
  `type mismatch`,
);

// ./test/core/memory_init.wast:1778
let $41 = instantiate(`(module
  (memory i64 1 1 )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i64) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:1796
assert_trap(() => invoke($41, `run`, [65528n, 16]), `out of bounds memory access`);

// ./test/core/memory_init.wast:1799
assert_return(() => invoke($41, `checkRange`, [0n, 1n, 0]), [value("i64", -1n)]);

// ./test/core/memory_init.wast:1801
let $42 = instantiate(`(module
  (memory i64 1 1 )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i64) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:1819
assert_trap(() => invoke($42, `run`, [65527n, 16]), `out of bounds memory access`);

// ./test/core/memory_init.wast:1822
assert_return(() => invoke($42, `checkRange`, [0n, 1n, 0]), [value("i64", -1n)]);

// ./test/core/memory_init.wast:1824
let $43 = instantiate(`(module
  (memory i64 1 1 )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i64) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:1842
assert_trap(() => invoke($43, `run`, [65472n, 30]), `out of bounds memory access`);

// ./test/core/memory_init.wast:1845
assert_return(() => invoke($43, `checkRange`, [0n, 1n, 0]), [value("i64", -1n)]);

// ./test/core/memory_init.wast:1847
let $44 = instantiate(`(module
  (memory i64 1 1 )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i64) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:1865
assert_trap(() => invoke($44, `run`, [65473n, 31]), `out of bounds memory access`);

// ./test/core/memory_init.wast:1868
assert_return(() => invoke($44, `checkRange`, [0n, 1n, 0]), [value("i64", -1n)]);

// ./test/core/memory_init.wast:1870
let $45 = instantiate(`(module
  (memory i64 1  )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i64) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:1888
assert_trap(() => invoke($45, `run`, [65528n, -256]), `out of bounds memory access`);

// ./test/core/memory_init.wast:1891
assert_return(() => invoke($45, `checkRange`, [0n, 1n, 0]), [value("i64", -1n)]);

// ./test/core/memory_init.wast:1893
let $46 = instantiate(`(module
  (memory i64 1  )
  (data "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
   
  (func (export "checkRange") (param $$from i64) (param $$to i64) (param $$expected i32) (result i64)
    (loop $$cont
      (if (i64.eq (local.get $$from) (local.get $$to))
        (then
          (return (i64.const -1))))
      (if (i32.eq (i32.load8_u (local.get $$from)) (local.get $$expected))
        (then
          (local.set $$from (i64.add (local.get $$from) (i64.const 1)))
          (br $$cont))))
    (return (local.get $$from)))

  (func (export "run") (param $$offs i64) (param $$len i32)
    (memory.init 0 (local.get $$offs) (i32.const 0) (local.get $$len))))`);

// ./test/core/memory_init.wast:1911
assert_trap(() => invoke($46, `run`, [0n, -4]), `out of bounds memory access`);

// ./test/core/memory_init.wast:1914
assert_return(() => invoke($46, `checkRange`, [0n, 1n, 0]), [value("i64", -1n)]);

// ./test/core/memory_init.wast:1917
let $47 = instantiate(`(module
  (memory i64 1)
  ;; 65 data segments. 64 is the smallest positive number that is encoded
  ;; differently as a signed LEB.
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "") (data "") (data "") (data "") (data "") (data "") (data "") (data "")
  (data "")
  (func (memory.init 64 (i64.const 0) (i32.const 0) (i32.const 0))))`);
