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

// ./test/core/table_copy.wast

// ./test/core/table_copy.wast:6
let $0 = instantiate(`(module
  (func (export "ef0") (result i32) (i32.const 0))
  (func (export "ef1") (result i32) (i32.const 1))
  (func (export "ef2") (result i32) (i32.const 2))
  (func (export "ef3") (result i32) (i32.const 3))
  (func (export "ef4") (result i32) (i32.const 4))
)`);

// ./test/core/table_copy.wast:13
register($0, `a`);

// ./test/core/table_copy.wast:15
let $1 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t1) (i32.const 3) func 1 3 1 4)
  (elem (table $$t1) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (nop))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:45
invoke($1, `test`, []);

// ./test/core/table_copy.wast:46
assert_trap(() => invoke($1, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:47
assert_trap(() => invoke($1, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:48
assert_return(() => invoke($1, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:49
assert_return(() => invoke($1, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:50
assert_return(() => invoke($1, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:51
assert_return(() => invoke($1, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:52
assert_trap(() => invoke($1, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:53
assert_trap(() => invoke($1, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:54
assert_trap(() => invoke($1, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:55
assert_trap(() => invoke($1, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:56
assert_trap(() => invoke($1, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:57
assert_trap(() => invoke($1, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:58
assert_return(() => invoke($1, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:59
assert_return(() => invoke($1, `check_t0`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:60
assert_return(() => invoke($1, `check_t0`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:61
assert_return(() => invoke($1, `check_t0`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:62
assert_return(() => invoke($1, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:63
assert_trap(() => invoke($1, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:64
assert_trap(() => invoke($1, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:65
assert_trap(() => invoke($1, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:66
assert_trap(() => invoke($1, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:67
assert_trap(() => invoke($1, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:68
assert_trap(() => invoke($1, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:69
assert_trap(() => invoke($1, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:70
assert_trap(() => invoke($1, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:71
assert_trap(() => invoke($1, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:72
assert_trap(() => invoke($1, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:73
assert_trap(() => invoke($1, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:74
assert_trap(() => invoke($1, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:75
assert_trap(() => invoke($1, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:76
assert_trap(() => invoke($1, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:77
assert_trap(() => invoke($1, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:78
assert_trap(() => invoke($1, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:79
assert_return(() => invoke($1, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:80
assert_return(() => invoke($1, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:81
assert_return(() => invoke($1, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:82
assert_return(() => invoke($1, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:83
assert_trap(() => invoke($1, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:84
assert_trap(() => invoke($1, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:85
assert_trap(() => invoke($1, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:86
assert_trap(() => invoke($1, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:87
assert_return(() => invoke($1, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:88
assert_return(() => invoke($1, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:89
assert_return(() => invoke($1, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:90
assert_return(() => invoke($1, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:91
assert_return(() => invoke($1, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:92
assert_trap(() => invoke($1, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:93
assert_trap(() => invoke($1, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:94
assert_trap(() => invoke($1, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:95
assert_trap(() => invoke($1, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:96
assert_trap(() => invoke($1, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:97
assert_trap(() => invoke($1, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:98
assert_trap(() => invoke($1, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:99
assert_trap(() => invoke($1, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:100
assert_trap(() => invoke($1, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:101
assert_trap(() => invoke($1, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:102
assert_trap(() => invoke($1, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:103
assert_trap(() => invoke($1, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:104
assert_trap(() => invoke($1, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:105
assert_trap(() => invoke($1, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:107
let $2 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t1) (i32.const 3) func 1 3 1 4)
  (elem (table $$t1) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 13) (i32.const 2) (i32.const 3)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:137
invoke($2, `test`, []);

// ./test/core/table_copy.wast:138
assert_trap(() => invoke($2, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:139
assert_trap(() => invoke($2, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:140
assert_return(() => invoke($2, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:141
assert_return(() => invoke($2, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:142
assert_return(() => invoke($2, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:143
assert_return(() => invoke($2, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:144
assert_trap(() => invoke($2, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:145
assert_trap(() => invoke($2, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:146
assert_trap(() => invoke($2, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:147
assert_trap(() => invoke($2, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:148
assert_trap(() => invoke($2, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:149
assert_trap(() => invoke($2, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:150
assert_return(() => invoke($2, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:151
assert_return(() => invoke($2, `check_t0`, [13]), [value("i32", 3)]);

// ./test/core/table_copy.wast:152
assert_return(() => invoke($2, `check_t0`, [14]), [value("i32", 1)]);

// ./test/core/table_copy.wast:153
assert_return(() => invoke($2, `check_t0`, [15]), [value("i32", 4)]);

// ./test/core/table_copy.wast:154
assert_return(() => invoke($2, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:155
assert_trap(() => invoke($2, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:156
assert_trap(() => invoke($2, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:157
assert_trap(() => invoke($2, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:158
assert_trap(() => invoke($2, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:159
assert_trap(() => invoke($2, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:160
assert_trap(() => invoke($2, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:161
assert_trap(() => invoke($2, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:162
assert_trap(() => invoke($2, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:163
assert_trap(() => invoke($2, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:164
assert_trap(() => invoke($2, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:165
assert_trap(() => invoke($2, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:166
assert_trap(() => invoke($2, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:167
assert_trap(() => invoke($2, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:168
assert_trap(() => invoke($2, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:169
assert_trap(() => invoke($2, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:170
assert_trap(() => invoke($2, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:171
assert_return(() => invoke($2, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:172
assert_return(() => invoke($2, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:173
assert_return(() => invoke($2, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:174
assert_return(() => invoke($2, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:175
assert_trap(() => invoke($2, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:176
assert_trap(() => invoke($2, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:177
assert_trap(() => invoke($2, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:178
assert_trap(() => invoke($2, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:179
assert_return(() => invoke($2, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:180
assert_return(() => invoke($2, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:181
assert_return(() => invoke($2, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:182
assert_return(() => invoke($2, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:183
assert_return(() => invoke($2, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:184
assert_trap(() => invoke($2, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:185
assert_trap(() => invoke($2, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:186
assert_trap(() => invoke($2, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:187
assert_trap(() => invoke($2, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:188
assert_trap(() => invoke($2, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:189
assert_trap(() => invoke($2, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:190
assert_trap(() => invoke($2, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:191
assert_trap(() => invoke($2, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:192
assert_trap(() => invoke($2, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:193
assert_trap(() => invoke($2, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:194
assert_trap(() => invoke($2, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:195
assert_trap(() => invoke($2, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:196
assert_trap(() => invoke($2, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:197
assert_trap(() => invoke($2, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:199
let $3 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t1) (i32.const 3) func 1 3 1 4)
  (elem (table $$t1) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 25) (i32.const 15) (i32.const 2)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:229
invoke($3, `test`, []);

// ./test/core/table_copy.wast:230
assert_trap(() => invoke($3, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:231
assert_trap(() => invoke($3, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:232
assert_return(() => invoke($3, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:233
assert_return(() => invoke($3, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:234
assert_return(() => invoke($3, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:235
assert_return(() => invoke($3, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:236
assert_trap(() => invoke($3, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:237
assert_trap(() => invoke($3, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:238
assert_trap(() => invoke($3, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:239
assert_trap(() => invoke($3, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:240
assert_trap(() => invoke($3, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:241
assert_trap(() => invoke($3, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:242
assert_return(() => invoke($3, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:243
assert_return(() => invoke($3, `check_t0`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:244
assert_return(() => invoke($3, `check_t0`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:245
assert_return(() => invoke($3, `check_t0`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:246
assert_return(() => invoke($3, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:247
assert_trap(() => invoke($3, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:248
assert_trap(() => invoke($3, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:249
assert_trap(() => invoke($3, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:250
assert_trap(() => invoke($3, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:251
assert_trap(() => invoke($3, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:252
assert_trap(() => invoke($3, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:253
assert_trap(() => invoke($3, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:254
assert_trap(() => invoke($3, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:255
assert_return(() => invoke($3, `check_t0`, [25]), [value("i32", 3)]);

// ./test/core/table_copy.wast:256
assert_return(() => invoke($3, `check_t0`, [26]), [value("i32", 6)]);

// ./test/core/table_copy.wast:257
assert_trap(() => invoke($3, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:258
assert_trap(() => invoke($3, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:259
assert_trap(() => invoke($3, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:260
assert_trap(() => invoke($3, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:261
assert_trap(() => invoke($3, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:262
assert_trap(() => invoke($3, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:263
assert_return(() => invoke($3, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:264
assert_return(() => invoke($3, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:265
assert_return(() => invoke($3, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:266
assert_return(() => invoke($3, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:267
assert_trap(() => invoke($3, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:268
assert_trap(() => invoke($3, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:269
assert_trap(() => invoke($3, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:270
assert_trap(() => invoke($3, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:271
assert_return(() => invoke($3, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:272
assert_return(() => invoke($3, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:273
assert_return(() => invoke($3, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:274
assert_return(() => invoke($3, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:275
assert_return(() => invoke($3, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:276
assert_trap(() => invoke($3, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:277
assert_trap(() => invoke($3, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:278
assert_trap(() => invoke($3, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:279
assert_trap(() => invoke($3, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:280
assert_trap(() => invoke($3, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:281
assert_trap(() => invoke($3, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:282
assert_trap(() => invoke($3, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:283
assert_trap(() => invoke($3, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:284
assert_trap(() => invoke($3, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:285
assert_trap(() => invoke($3, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:286
assert_trap(() => invoke($3, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:287
assert_trap(() => invoke($3, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:288
assert_trap(() => invoke($3, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:289
assert_trap(() => invoke($3, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:291
let $4 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t1) (i32.const 3) func 1 3 1 4)
  (elem (table $$t1) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 13) (i32.const 25) (i32.const 3)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:321
invoke($4, `test`, []);

// ./test/core/table_copy.wast:322
assert_trap(() => invoke($4, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:323
assert_trap(() => invoke($4, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:324
assert_return(() => invoke($4, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:325
assert_return(() => invoke($4, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:326
assert_return(() => invoke($4, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:327
assert_return(() => invoke($4, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:328
assert_trap(() => invoke($4, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:329
assert_trap(() => invoke($4, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:330
assert_trap(() => invoke($4, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:331
assert_trap(() => invoke($4, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:332
assert_trap(() => invoke($4, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:333
assert_trap(() => invoke($4, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:334
assert_return(() => invoke($4, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:335
assert_trap(() => invoke($4, `check_t0`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:336
assert_trap(() => invoke($4, `check_t0`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:337
assert_trap(() => invoke($4, `check_t0`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:338
assert_return(() => invoke($4, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:339
assert_trap(() => invoke($4, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:340
assert_trap(() => invoke($4, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:341
assert_trap(() => invoke($4, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:342
assert_trap(() => invoke($4, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:343
assert_trap(() => invoke($4, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:344
assert_trap(() => invoke($4, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:345
assert_trap(() => invoke($4, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:346
assert_trap(() => invoke($4, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:347
assert_trap(() => invoke($4, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:348
assert_trap(() => invoke($4, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:349
assert_trap(() => invoke($4, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:350
assert_trap(() => invoke($4, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:351
assert_trap(() => invoke($4, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:352
assert_trap(() => invoke($4, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:353
assert_trap(() => invoke($4, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:354
assert_trap(() => invoke($4, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:355
assert_return(() => invoke($4, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:356
assert_return(() => invoke($4, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:357
assert_return(() => invoke($4, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:358
assert_return(() => invoke($4, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:359
assert_trap(() => invoke($4, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:360
assert_trap(() => invoke($4, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:361
assert_trap(() => invoke($4, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:362
assert_trap(() => invoke($4, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:363
assert_return(() => invoke($4, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:364
assert_return(() => invoke($4, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:365
assert_return(() => invoke($4, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:366
assert_return(() => invoke($4, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:367
assert_return(() => invoke($4, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:368
assert_trap(() => invoke($4, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:369
assert_trap(() => invoke($4, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:370
assert_trap(() => invoke($4, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:371
assert_trap(() => invoke($4, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:372
assert_trap(() => invoke($4, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:373
assert_trap(() => invoke($4, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:374
assert_trap(() => invoke($4, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:375
assert_trap(() => invoke($4, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:376
assert_trap(() => invoke($4, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:377
assert_trap(() => invoke($4, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:378
assert_trap(() => invoke($4, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:379
assert_trap(() => invoke($4, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:380
assert_trap(() => invoke($4, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:381
assert_trap(() => invoke($4, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:383
let $5 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t1) (i32.const 3) func 1 3 1 4)
  (elem (table $$t1) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 20) (i32.const 22) (i32.const 4)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:413
invoke($5, `test`, []);

// ./test/core/table_copy.wast:414
assert_trap(() => invoke($5, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:415
assert_trap(() => invoke($5, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:416
assert_return(() => invoke($5, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:417
assert_return(() => invoke($5, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:418
assert_return(() => invoke($5, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:419
assert_return(() => invoke($5, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:420
assert_trap(() => invoke($5, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:421
assert_trap(() => invoke($5, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:422
assert_trap(() => invoke($5, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:423
assert_trap(() => invoke($5, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:424
assert_trap(() => invoke($5, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:425
assert_trap(() => invoke($5, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:426
assert_return(() => invoke($5, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:427
assert_return(() => invoke($5, `check_t0`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:428
assert_return(() => invoke($5, `check_t0`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:429
assert_return(() => invoke($5, `check_t0`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:430
assert_return(() => invoke($5, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:431
assert_trap(() => invoke($5, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:432
assert_trap(() => invoke($5, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:433
assert_trap(() => invoke($5, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:434
assert_trap(() => invoke($5, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:435
assert_trap(() => invoke($5, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:436
assert_trap(() => invoke($5, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:437
assert_trap(() => invoke($5, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:438
assert_trap(() => invoke($5, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:439
assert_trap(() => invoke($5, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:440
assert_trap(() => invoke($5, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:441
assert_trap(() => invoke($5, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:442
assert_trap(() => invoke($5, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:443
assert_trap(() => invoke($5, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:444
assert_trap(() => invoke($5, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:445
assert_trap(() => invoke($5, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:446
assert_trap(() => invoke($5, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:447
assert_return(() => invoke($5, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:448
assert_return(() => invoke($5, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:449
assert_return(() => invoke($5, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:450
assert_return(() => invoke($5, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:451
assert_trap(() => invoke($5, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:452
assert_trap(() => invoke($5, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:453
assert_trap(() => invoke($5, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:454
assert_trap(() => invoke($5, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:455
assert_return(() => invoke($5, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:456
assert_return(() => invoke($5, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:457
assert_return(() => invoke($5, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:458
assert_return(() => invoke($5, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:459
assert_return(() => invoke($5, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:460
assert_trap(() => invoke($5, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:461
assert_trap(() => invoke($5, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:462
assert_trap(() => invoke($5, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:463
assert_trap(() => invoke($5, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:464
assert_trap(() => invoke($5, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:465
assert_trap(() => invoke($5, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:466
assert_trap(() => invoke($5, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:467
assert_trap(() => invoke($5, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:468
assert_trap(() => invoke($5, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:469
assert_trap(() => invoke($5, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:470
assert_trap(() => invoke($5, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:471
assert_trap(() => invoke($5, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:472
assert_trap(() => invoke($5, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:473
assert_trap(() => invoke($5, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:475
let $6 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t1) (i32.const 3) func 1 3 1 4)
  (elem (table $$t1) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 25) (i32.const 1) (i32.const 3)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:505
invoke($6, `test`, []);

// ./test/core/table_copy.wast:506
assert_trap(() => invoke($6, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:507
assert_trap(() => invoke($6, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:508
assert_return(() => invoke($6, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:509
assert_return(() => invoke($6, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:510
assert_return(() => invoke($6, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:511
assert_return(() => invoke($6, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:512
assert_trap(() => invoke($6, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:513
assert_trap(() => invoke($6, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:514
assert_trap(() => invoke($6, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:515
assert_trap(() => invoke($6, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:516
assert_trap(() => invoke($6, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:517
assert_trap(() => invoke($6, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:518
assert_return(() => invoke($6, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:519
assert_return(() => invoke($6, `check_t0`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:520
assert_return(() => invoke($6, `check_t0`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:521
assert_return(() => invoke($6, `check_t0`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:522
assert_return(() => invoke($6, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:523
assert_trap(() => invoke($6, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:524
assert_trap(() => invoke($6, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:525
assert_trap(() => invoke($6, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:526
assert_trap(() => invoke($6, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:527
assert_trap(() => invoke($6, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:528
assert_trap(() => invoke($6, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:529
assert_trap(() => invoke($6, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:530
assert_trap(() => invoke($6, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:531
assert_trap(() => invoke($6, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:532
assert_return(() => invoke($6, `check_t0`, [26]), [value("i32", 3)]);

// ./test/core/table_copy.wast:533
assert_return(() => invoke($6, `check_t0`, [27]), [value("i32", 1)]);

// ./test/core/table_copy.wast:534
assert_trap(() => invoke($6, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:535
assert_trap(() => invoke($6, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:536
assert_trap(() => invoke($6, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:537
assert_trap(() => invoke($6, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:538
assert_trap(() => invoke($6, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:539
assert_return(() => invoke($6, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:540
assert_return(() => invoke($6, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:541
assert_return(() => invoke($6, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:542
assert_return(() => invoke($6, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:543
assert_trap(() => invoke($6, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:544
assert_trap(() => invoke($6, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:545
assert_trap(() => invoke($6, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:546
assert_trap(() => invoke($6, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:547
assert_return(() => invoke($6, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:548
assert_return(() => invoke($6, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:549
assert_return(() => invoke($6, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:550
assert_return(() => invoke($6, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:551
assert_return(() => invoke($6, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:552
assert_trap(() => invoke($6, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:553
assert_trap(() => invoke($6, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:554
assert_trap(() => invoke($6, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:555
assert_trap(() => invoke($6, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:556
assert_trap(() => invoke($6, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:557
assert_trap(() => invoke($6, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:558
assert_trap(() => invoke($6, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:559
assert_trap(() => invoke($6, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:560
assert_trap(() => invoke($6, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:561
assert_trap(() => invoke($6, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:562
assert_trap(() => invoke($6, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:563
assert_trap(() => invoke($6, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:564
assert_trap(() => invoke($6, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:565
assert_trap(() => invoke($6, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:567
let $7 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t1) (i32.const 3) func 1 3 1 4)
  (elem (table $$t1) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 10) (i32.const 12) (i32.const 7)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:597
invoke($7, `test`, []);

// ./test/core/table_copy.wast:598
assert_trap(() => invoke($7, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:599
assert_trap(() => invoke($7, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:600
assert_return(() => invoke($7, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:601
assert_return(() => invoke($7, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:602
assert_return(() => invoke($7, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:603
assert_return(() => invoke($7, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:604
assert_trap(() => invoke($7, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:605
assert_trap(() => invoke($7, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:606
assert_trap(() => invoke($7, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:607
assert_trap(() => invoke($7, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:608
assert_return(() => invoke($7, `check_t0`, [10]), [value("i32", 7)]);

// ./test/core/table_copy.wast:609
assert_return(() => invoke($7, `check_t0`, [11]), [value("i32", 5)]);

// ./test/core/table_copy.wast:610
assert_return(() => invoke($7, `check_t0`, [12]), [value("i32", 2)]);

// ./test/core/table_copy.wast:611
assert_return(() => invoke($7, `check_t0`, [13]), [value("i32", 3)]);

// ./test/core/table_copy.wast:612
assert_return(() => invoke($7, `check_t0`, [14]), [value("i32", 6)]);

// ./test/core/table_copy.wast:613
assert_trap(() => invoke($7, `check_t0`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:614
assert_trap(() => invoke($7, `check_t0`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:615
assert_trap(() => invoke($7, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:616
assert_trap(() => invoke($7, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:617
assert_trap(() => invoke($7, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:618
assert_trap(() => invoke($7, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:619
assert_trap(() => invoke($7, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:620
assert_trap(() => invoke($7, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:621
assert_trap(() => invoke($7, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:622
assert_trap(() => invoke($7, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:623
assert_trap(() => invoke($7, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:624
assert_trap(() => invoke($7, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:625
assert_trap(() => invoke($7, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:626
assert_trap(() => invoke($7, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:627
assert_trap(() => invoke($7, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:628
assert_trap(() => invoke($7, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:629
assert_trap(() => invoke($7, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:630
assert_trap(() => invoke($7, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:631
assert_return(() => invoke($7, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:632
assert_return(() => invoke($7, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:633
assert_return(() => invoke($7, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:634
assert_return(() => invoke($7, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:635
assert_trap(() => invoke($7, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:636
assert_trap(() => invoke($7, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:637
assert_trap(() => invoke($7, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:638
assert_trap(() => invoke($7, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:639
assert_return(() => invoke($7, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:640
assert_return(() => invoke($7, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:641
assert_return(() => invoke($7, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:642
assert_return(() => invoke($7, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:643
assert_return(() => invoke($7, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:644
assert_trap(() => invoke($7, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:645
assert_trap(() => invoke($7, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:646
assert_trap(() => invoke($7, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:647
assert_trap(() => invoke($7, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:648
assert_trap(() => invoke($7, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:649
assert_trap(() => invoke($7, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:650
assert_trap(() => invoke($7, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:651
assert_trap(() => invoke($7, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:652
assert_trap(() => invoke($7, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:653
assert_trap(() => invoke($7, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:654
assert_trap(() => invoke($7, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:655
assert_trap(() => invoke($7, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:656
assert_trap(() => invoke($7, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:657
assert_trap(() => invoke($7, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:659
let $8 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t1) (i32.const 3) func 1 3 1 4)
  (elem (table $$t1) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 12) (i32.const 10) (i32.const 7)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:689
invoke($8, `test`, []);

// ./test/core/table_copy.wast:690
assert_trap(() => invoke($8, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:691
assert_trap(() => invoke($8, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:692
assert_return(() => invoke($8, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:693
assert_return(() => invoke($8, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:694
assert_return(() => invoke($8, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:695
assert_return(() => invoke($8, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:696
assert_trap(() => invoke($8, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:697
assert_trap(() => invoke($8, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:698
assert_trap(() => invoke($8, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:699
assert_trap(() => invoke($8, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:700
assert_trap(() => invoke($8, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:701
assert_trap(() => invoke($8, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:702
assert_trap(() => invoke($8, `check_t0`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:703
assert_trap(() => invoke($8, `check_t0`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:704
assert_return(() => invoke($8, `check_t0`, [14]), [value("i32", 7)]);

// ./test/core/table_copy.wast:705
assert_return(() => invoke($8, `check_t0`, [15]), [value("i32", 5)]);

// ./test/core/table_copy.wast:706
assert_return(() => invoke($8, `check_t0`, [16]), [value("i32", 2)]);

// ./test/core/table_copy.wast:707
assert_return(() => invoke($8, `check_t0`, [17]), [value("i32", 3)]);

// ./test/core/table_copy.wast:708
assert_return(() => invoke($8, `check_t0`, [18]), [value("i32", 6)]);

// ./test/core/table_copy.wast:709
assert_trap(() => invoke($8, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:710
assert_trap(() => invoke($8, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:711
assert_trap(() => invoke($8, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:712
assert_trap(() => invoke($8, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:713
assert_trap(() => invoke($8, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:714
assert_trap(() => invoke($8, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:715
assert_trap(() => invoke($8, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:716
assert_trap(() => invoke($8, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:717
assert_trap(() => invoke($8, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:718
assert_trap(() => invoke($8, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:719
assert_trap(() => invoke($8, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:720
assert_trap(() => invoke($8, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:721
assert_trap(() => invoke($8, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:722
assert_trap(() => invoke($8, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:723
assert_return(() => invoke($8, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:724
assert_return(() => invoke($8, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:725
assert_return(() => invoke($8, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:726
assert_return(() => invoke($8, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:727
assert_trap(() => invoke($8, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:728
assert_trap(() => invoke($8, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:729
assert_trap(() => invoke($8, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:730
assert_trap(() => invoke($8, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:731
assert_return(() => invoke($8, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:732
assert_return(() => invoke($8, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:733
assert_return(() => invoke($8, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:734
assert_return(() => invoke($8, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:735
assert_return(() => invoke($8, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:736
assert_trap(() => invoke($8, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:737
assert_trap(() => invoke($8, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:738
assert_trap(() => invoke($8, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:739
assert_trap(() => invoke($8, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:740
assert_trap(() => invoke($8, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:741
assert_trap(() => invoke($8, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:742
assert_trap(() => invoke($8, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:743
assert_trap(() => invoke($8, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:744
assert_trap(() => invoke($8, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:745
assert_trap(() => invoke($8, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:746
assert_trap(() => invoke($8, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:747
assert_trap(() => invoke($8, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:748
assert_trap(() => invoke($8, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:749
assert_trap(() => invoke($8, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:751
let $9 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t1) (i32.const 3) func 1 3 1 4)
  (elem (table $$t1) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t1 $$t0 (i32.const 10) (i32.const 0) (i32.const 20)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:781
invoke($9, `test`, []);

// ./test/core/table_copy.wast:782
assert_trap(() => invoke($9, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:783
assert_trap(() => invoke($9, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:784
assert_return(() => invoke($9, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:785
assert_return(() => invoke($9, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:786
assert_return(() => invoke($9, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:787
assert_return(() => invoke($9, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:788
assert_trap(() => invoke($9, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:789
assert_trap(() => invoke($9, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:790
assert_trap(() => invoke($9, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:791
assert_trap(() => invoke($9, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:792
assert_trap(() => invoke($9, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:793
assert_trap(() => invoke($9, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:794
assert_return(() => invoke($9, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:795
assert_return(() => invoke($9, `check_t0`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:796
assert_return(() => invoke($9, `check_t0`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:797
assert_return(() => invoke($9, `check_t0`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:798
assert_return(() => invoke($9, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:799
assert_trap(() => invoke($9, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:800
assert_trap(() => invoke($9, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:801
assert_trap(() => invoke($9, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:802
assert_trap(() => invoke($9, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:803
assert_trap(() => invoke($9, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:804
assert_trap(() => invoke($9, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:805
assert_trap(() => invoke($9, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:806
assert_trap(() => invoke($9, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:807
assert_trap(() => invoke($9, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:808
assert_trap(() => invoke($9, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:809
assert_trap(() => invoke($9, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:810
assert_trap(() => invoke($9, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:811
assert_trap(() => invoke($9, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:812
assert_trap(() => invoke($9, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:813
assert_trap(() => invoke($9, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:814
assert_trap(() => invoke($9, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:815
assert_return(() => invoke($9, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:816
assert_return(() => invoke($9, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:817
assert_return(() => invoke($9, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:818
assert_return(() => invoke($9, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:819
assert_trap(() => invoke($9, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:820
assert_trap(() => invoke($9, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:821
assert_trap(() => invoke($9, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:822
assert_trap(() => invoke($9, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:823
assert_trap(() => invoke($9, `check_t1`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:824
assert_return(() => invoke($9, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:825
assert_return(() => invoke($9, `check_t1`, [13]), [value("i32", 1)]);

// ./test/core/table_copy.wast:826
assert_return(() => invoke($9, `check_t1`, [14]), [value("i32", 4)]);

// ./test/core/table_copy.wast:827
assert_return(() => invoke($9, `check_t1`, [15]), [value("i32", 1)]);

// ./test/core/table_copy.wast:828
assert_trap(() => invoke($9, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:829
assert_trap(() => invoke($9, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:830
assert_trap(() => invoke($9, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:831
assert_trap(() => invoke($9, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:832
assert_trap(() => invoke($9, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:833
assert_trap(() => invoke($9, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:834
assert_return(() => invoke($9, `check_t1`, [22]), [value("i32", 7)]);

// ./test/core/table_copy.wast:835
assert_return(() => invoke($9, `check_t1`, [23]), [value("i32", 5)]);

// ./test/core/table_copy.wast:836
assert_return(() => invoke($9, `check_t1`, [24]), [value("i32", 2)]);

// ./test/core/table_copy.wast:837
assert_return(() => invoke($9, `check_t1`, [25]), [value("i32", 3)]);

// ./test/core/table_copy.wast:838
assert_return(() => invoke($9, `check_t1`, [26]), [value("i32", 6)]);

// ./test/core/table_copy.wast:839
assert_trap(() => invoke($9, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:840
assert_trap(() => invoke($9, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:841
assert_trap(() => invoke($9, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:843
let $10 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t0) (i32.const 3) func 1 3 1 4)
  (elem (table $$t0) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (nop))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:873
invoke($10, `test`, []);

// ./test/core/table_copy.wast:874
assert_trap(() => invoke($10, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:875
assert_trap(() => invoke($10, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:876
assert_return(() => invoke($10, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:877
assert_return(() => invoke($10, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:878
assert_return(() => invoke($10, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:879
assert_return(() => invoke($10, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:880
assert_trap(() => invoke($10, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:881
assert_trap(() => invoke($10, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:882
assert_trap(() => invoke($10, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:883
assert_trap(() => invoke($10, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:884
assert_trap(() => invoke($10, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:885
assert_trap(() => invoke($10, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:886
assert_return(() => invoke($10, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:887
assert_return(() => invoke($10, `check_t0`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:888
assert_return(() => invoke($10, `check_t0`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:889
assert_return(() => invoke($10, `check_t0`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:890
assert_return(() => invoke($10, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:891
assert_trap(() => invoke($10, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:892
assert_trap(() => invoke($10, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:893
assert_trap(() => invoke($10, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:894
assert_trap(() => invoke($10, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:895
assert_trap(() => invoke($10, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:896
assert_trap(() => invoke($10, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:897
assert_trap(() => invoke($10, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:898
assert_trap(() => invoke($10, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:899
assert_trap(() => invoke($10, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:900
assert_trap(() => invoke($10, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:901
assert_trap(() => invoke($10, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:902
assert_trap(() => invoke($10, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:903
assert_trap(() => invoke($10, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:904
assert_trap(() => invoke($10, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:905
assert_trap(() => invoke($10, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:906
assert_trap(() => invoke($10, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:907
assert_return(() => invoke($10, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:908
assert_return(() => invoke($10, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:909
assert_return(() => invoke($10, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:910
assert_return(() => invoke($10, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:911
assert_trap(() => invoke($10, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:912
assert_trap(() => invoke($10, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:913
assert_trap(() => invoke($10, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:914
assert_trap(() => invoke($10, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:915
assert_return(() => invoke($10, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:916
assert_return(() => invoke($10, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:917
assert_return(() => invoke($10, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:918
assert_return(() => invoke($10, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:919
assert_return(() => invoke($10, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:920
assert_trap(() => invoke($10, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:921
assert_trap(() => invoke($10, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:922
assert_trap(() => invoke($10, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:923
assert_trap(() => invoke($10, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:924
assert_trap(() => invoke($10, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:925
assert_trap(() => invoke($10, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:926
assert_trap(() => invoke($10, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:927
assert_trap(() => invoke($10, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:928
assert_trap(() => invoke($10, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:929
assert_trap(() => invoke($10, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:930
assert_trap(() => invoke($10, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:931
assert_trap(() => invoke($10, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:932
assert_trap(() => invoke($10, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:933
assert_trap(() => invoke($10, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:935
let $11 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t0) (i32.const 3) func 1 3 1 4)
  (elem (table $$t0) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t1 $$t1 (i32.const 13) (i32.const 2) (i32.const 3)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:965
invoke($11, `test`, []);

// ./test/core/table_copy.wast:966
assert_trap(() => invoke($11, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:967
assert_trap(() => invoke($11, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:968
assert_return(() => invoke($11, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:969
assert_return(() => invoke($11, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:970
assert_return(() => invoke($11, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:971
assert_return(() => invoke($11, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:972
assert_trap(() => invoke($11, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:973
assert_trap(() => invoke($11, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:974
assert_trap(() => invoke($11, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:975
assert_trap(() => invoke($11, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:976
assert_trap(() => invoke($11, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:977
assert_trap(() => invoke($11, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:978
assert_return(() => invoke($11, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:979
assert_return(() => invoke($11, `check_t0`, [13]), [value("i32", 3)]);

// ./test/core/table_copy.wast:980
assert_return(() => invoke($11, `check_t0`, [14]), [value("i32", 1)]);

// ./test/core/table_copy.wast:981
assert_return(() => invoke($11, `check_t0`, [15]), [value("i32", 4)]);

// ./test/core/table_copy.wast:982
assert_return(() => invoke($11, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:983
assert_trap(() => invoke($11, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:984
assert_trap(() => invoke($11, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:985
assert_trap(() => invoke($11, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:986
assert_trap(() => invoke($11, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:987
assert_trap(() => invoke($11, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:988
assert_trap(() => invoke($11, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:989
assert_trap(() => invoke($11, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:990
assert_trap(() => invoke($11, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:991
assert_trap(() => invoke($11, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:992
assert_trap(() => invoke($11, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:993
assert_trap(() => invoke($11, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:994
assert_trap(() => invoke($11, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:995
assert_trap(() => invoke($11, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:996
assert_trap(() => invoke($11, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:997
assert_trap(() => invoke($11, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:998
assert_trap(() => invoke($11, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:999
assert_return(() => invoke($11, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1000
assert_return(() => invoke($11, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1001
assert_return(() => invoke($11, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1002
assert_return(() => invoke($11, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1003
assert_trap(() => invoke($11, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1004
assert_trap(() => invoke($11, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1005
assert_trap(() => invoke($11, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1006
assert_trap(() => invoke($11, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1007
assert_return(() => invoke($11, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1008
assert_return(() => invoke($11, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1009
assert_return(() => invoke($11, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1010
assert_return(() => invoke($11, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1011
assert_return(() => invoke($11, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1012
assert_trap(() => invoke($11, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1013
assert_trap(() => invoke($11, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1014
assert_trap(() => invoke($11, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1015
assert_trap(() => invoke($11, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1016
assert_trap(() => invoke($11, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1017
assert_trap(() => invoke($11, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1018
assert_trap(() => invoke($11, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1019
assert_trap(() => invoke($11, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1020
assert_trap(() => invoke($11, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1021
assert_trap(() => invoke($11, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1022
assert_trap(() => invoke($11, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1023
assert_trap(() => invoke($11, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1024
assert_trap(() => invoke($11, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1025
assert_trap(() => invoke($11, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1027
let $12 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t0) (i32.const 3) func 1 3 1 4)
  (elem (table $$t0) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t1 $$t1 (i32.const 25) (i32.const 15) (i32.const 2)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:1057
invoke($12, `test`, []);

// ./test/core/table_copy.wast:1058
assert_trap(() => invoke($12, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1059
assert_trap(() => invoke($12, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1060
assert_return(() => invoke($12, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1061
assert_return(() => invoke($12, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1062
assert_return(() => invoke($12, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1063
assert_return(() => invoke($12, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1064
assert_trap(() => invoke($12, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1065
assert_trap(() => invoke($12, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1066
assert_trap(() => invoke($12, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1067
assert_trap(() => invoke($12, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1068
assert_trap(() => invoke($12, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1069
assert_trap(() => invoke($12, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:1070
assert_return(() => invoke($12, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1071
assert_return(() => invoke($12, `check_t0`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1072
assert_return(() => invoke($12, `check_t0`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1073
assert_return(() => invoke($12, `check_t0`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1074
assert_return(() => invoke($12, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1075
assert_trap(() => invoke($12, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1076
assert_trap(() => invoke($12, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1077
assert_trap(() => invoke($12, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1078
assert_trap(() => invoke($12, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1079
assert_trap(() => invoke($12, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1080
assert_trap(() => invoke($12, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1081
assert_trap(() => invoke($12, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1082
assert_trap(() => invoke($12, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1083
assert_return(() => invoke($12, `check_t0`, [25]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1084
assert_return(() => invoke($12, `check_t0`, [26]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1085
assert_trap(() => invoke($12, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1086
assert_trap(() => invoke($12, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1087
assert_trap(() => invoke($12, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1088
assert_trap(() => invoke($12, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1089
assert_trap(() => invoke($12, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1090
assert_trap(() => invoke($12, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1091
assert_return(() => invoke($12, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1092
assert_return(() => invoke($12, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1093
assert_return(() => invoke($12, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1094
assert_return(() => invoke($12, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1095
assert_trap(() => invoke($12, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1096
assert_trap(() => invoke($12, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1097
assert_trap(() => invoke($12, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1098
assert_trap(() => invoke($12, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1099
assert_return(() => invoke($12, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1100
assert_return(() => invoke($12, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1101
assert_return(() => invoke($12, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1102
assert_return(() => invoke($12, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1103
assert_return(() => invoke($12, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1104
assert_trap(() => invoke($12, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1105
assert_trap(() => invoke($12, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1106
assert_trap(() => invoke($12, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1107
assert_trap(() => invoke($12, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1108
assert_trap(() => invoke($12, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1109
assert_trap(() => invoke($12, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1110
assert_trap(() => invoke($12, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1111
assert_trap(() => invoke($12, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1112
assert_trap(() => invoke($12, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1113
assert_trap(() => invoke($12, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1114
assert_trap(() => invoke($12, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1115
assert_trap(() => invoke($12, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1116
assert_trap(() => invoke($12, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1117
assert_trap(() => invoke($12, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1119
let $13 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t0) (i32.const 3) func 1 3 1 4)
  (elem (table $$t0) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t1 $$t1 (i32.const 13) (i32.const 25) (i32.const 3)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:1149
invoke($13, `test`, []);

// ./test/core/table_copy.wast:1150
assert_trap(() => invoke($13, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1151
assert_trap(() => invoke($13, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1152
assert_return(() => invoke($13, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1153
assert_return(() => invoke($13, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1154
assert_return(() => invoke($13, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1155
assert_return(() => invoke($13, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1156
assert_trap(() => invoke($13, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1157
assert_trap(() => invoke($13, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1158
assert_trap(() => invoke($13, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1159
assert_trap(() => invoke($13, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1160
assert_trap(() => invoke($13, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1161
assert_trap(() => invoke($13, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:1162
assert_return(() => invoke($13, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1163
assert_trap(() => invoke($13, `check_t0`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:1164
assert_trap(() => invoke($13, `check_t0`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:1165
assert_trap(() => invoke($13, `check_t0`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:1166
assert_return(() => invoke($13, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1167
assert_trap(() => invoke($13, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1168
assert_trap(() => invoke($13, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1169
assert_trap(() => invoke($13, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1170
assert_trap(() => invoke($13, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1171
assert_trap(() => invoke($13, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1172
assert_trap(() => invoke($13, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1173
assert_trap(() => invoke($13, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1174
assert_trap(() => invoke($13, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1175
assert_trap(() => invoke($13, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1176
assert_trap(() => invoke($13, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1177
assert_trap(() => invoke($13, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1178
assert_trap(() => invoke($13, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1179
assert_trap(() => invoke($13, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1180
assert_trap(() => invoke($13, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1181
assert_trap(() => invoke($13, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1182
assert_trap(() => invoke($13, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1183
assert_return(() => invoke($13, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1184
assert_return(() => invoke($13, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1185
assert_return(() => invoke($13, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1186
assert_return(() => invoke($13, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1187
assert_trap(() => invoke($13, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1188
assert_trap(() => invoke($13, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1189
assert_trap(() => invoke($13, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1190
assert_trap(() => invoke($13, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1191
assert_return(() => invoke($13, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1192
assert_return(() => invoke($13, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1193
assert_return(() => invoke($13, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1194
assert_return(() => invoke($13, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1195
assert_return(() => invoke($13, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1196
assert_trap(() => invoke($13, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1197
assert_trap(() => invoke($13, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1198
assert_trap(() => invoke($13, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1199
assert_trap(() => invoke($13, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1200
assert_trap(() => invoke($13, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1201
assert_trap(() => invoke($13, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1202
assert_trap(() => invoke($13, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1203
assert_trap(() => invoke($13, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1204
assert_trap(() => invoke($13, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1205
assert_trap(() => invoke($13, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1206
assert_trap(() => invoke($13, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1207
assert_trap(() => invoke($13, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1208
assert_trap(() => invoke($13, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1209
assert_trap(() => invoke($13, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1211
let $14 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t0) (i32.const 3) func 1 3 1 4)
  (elem (table $$t0) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t1 $$t1 (i32.const 20) (i32.const 22) (i32.const 4)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:1241
invoke($14, `test`, []);

// ./test/core/table_copy.wast:1242
assert_trap(() => invoke($14, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1243
assert_trap(() => invoke($14, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1244
assert_return(() => invoke($14, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1245
assert_return(() => invoke($14, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1246
assert_return(() => invoke($14, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1247
assert_return(() => invoke($14, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1248
assert_trap(() => invoke($14, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1249
assert_trap(() => invoke($14, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1250
assert_trap(() => invoke($14, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1251
assert_trap(() => invoke($14, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1252
assert_trap(() => invoke($14, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1253
assert_trap(() => invoke($14, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:1254
assert_return(() => invoke($14, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1255
assert_return(() => invoke($14, `check_t0`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1256
assert_return(() => invoke($14, `check_t0`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1257
assert_return(() => invoke($14, `check_t0`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1258
assert_return(() => invoke($14, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1259
assert_trap(() => invoke($14, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1260
assert_trap(() => invoke($14, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1261
assert_trap(() => invoke($14, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1262
assert_trap(() => invoke($14, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1263
assert_trap(() => invoke($14, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1264
assert_trap(() => invoke($14, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1265
assert_trap(() => invoke($14, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1266
assert_trap(() => invoke($14, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1267
assert_trap(() => invoke($14, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1268
assert_trap(() => invoke($14, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1269
assert_trap(() => invoke($14, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1270
assert_trap(() => invoke($14, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1271
assert_trap(() => invoke($14, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1272
assert_trap(() => invoke($14, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1273
assert_trap(() => invoke($14, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1274
assert_trap(() => invoke($14, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1275
assert_return(() => invoke($14, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1276
assert_return(() => invoke($14, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1277
assert_return(() => invoke($14, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1278
assert_return(() => invoke($14, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1279
assert_trap(() => invoke($14, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1280
assert_trap(() => invoke($14, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1281
assert_trap(() => invoke($14, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1282
assert_trap(() => invoke($14, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1283
assert_return(() => invoke($14, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1284
assert_return(() => invoke($14, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1285
assert_return(() => invoke($14, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1286
assert_return(() => invoke($14, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1287
assert_return(() => invoke($14, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1288
assert_trap(() => invoke($14, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1289
assert_trap(() => invoke($14, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1290
assert_trap(() => invoke($14, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1291
assert_trap(() => invoke($14, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1292
assert_trap(() => invoke($14, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1293
assert_trap(() => invoke($14, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1294
assert_trap(() => invoke($14, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1295
assert_trap(() => invoke($14, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1296
assert_trap(() => invoke($14, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1297
assert_trap(() => invoke($14, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1298
assert_trap(() => invoke($14, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1299
assert_trap(() => invoke($14, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1300
assert_trap(() => invoke($14, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1301
assert_trap(() => invoke($14, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1303
let $15 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t0) (i32.const 3) func 1 3 1 4)
  (elem (table $$t0) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t1 $$t1 (i32.const 25) (i32.const 1) (i32.const 3)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:1333
invoke($15, `test`, []);

// ./test/core/table_copy.wast:1334
assert_trap(() => invoke($15, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1335
assert_trap(() => invoke($15, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1336
assert_return(() => invoke($15, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1337
assert_return(() => invoke($15, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1338
assert_return(() => invoke($15, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1339
assert_return(() => invoke($15, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1340
assert_trap(() => invoke($15, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1341
assert_trap(() => invoke($15, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1342
assert_trap(() => invoke($15, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1343
assert_trap(() => invoke($15, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1344
assert_trap(() => invoke($15, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1345
assert_trap(() => invoke($15, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:1346
assert_return(() => invoke($15, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1347
assert_return(() => invoke($15, `check_t0`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1348
assert_return(() => invoke($15, `check_t0`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1349
assert_return(() => invoke($15, `check_t0`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1350
assert_return(() => invoke($15, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1351
assert_trap(() => invoke($15, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1352
assert_trap(() => invoke($15, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1353
assert_trap(() => invoke($15, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1354
assert_trap(() => invoke($15, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1355
assert_trap(() => invoke($15, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1356
assert_trap(() => invoke($15, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1357
assert_trap(() => invoke($15, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1358
assert_trap(() => invoke($15, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1359
assert_trap(() => invoke($15, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1360
assert_return(() => invoke($15, `check_t0`, [26]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1361
assert_return(() => invoke($15, `check_t0`, [27]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1362
assert_trap(() => invoke($15, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1363
assert_trap(() => invoke($15, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1364
assert_trap(() => invoke($15, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1365
assert_trap(() => invoke($15, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1366
assert_trap(() => invoke($15, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1367
assert_return(() => invoke($15, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1368
assert_return(() => invoke($15, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1369
assert_return(() => invoke($15, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1370
assert_return(() => invoke($15, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1371
assert_trap(() => invoke($15, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1372
assert_trap(() => invoke($15, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1373
assert_trap(() => invoke($15, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1374
assert_trap(() => invoke($15, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1375
assert_return(() => invoke($15, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1376
assert_return(() => invoke($15, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1377
assert_return(() => invoke($15, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1378
assert_return(() => invoke($15, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1379
assert_return(() => invoke($15, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1380
assert_trap(() => invoke($15, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1381
assert_trap(() => invoke($15, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1382
assert_trap(() => invoke($15, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1383
assert_trap(() => invoke($15, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1384
assert_trap(() => invoke($15, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1385
assert_trap(() => invoke($15, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1386
assert_trap(() => invoke($15, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1387
assert_trap(() => invoke($15, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1388
assert_trap(() => invoke($15, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1389
assert_trap(() => invoke($15, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1390
assert_trap(() => invoke($15, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1391
assert_trap(() => invoke($15, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1392
assert_trap(() => invoke($15, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1393
assert_trap(() => invoke($15, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1395
let $16 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t0) (i32.const 3) func 1 3 1 4)
  (elem (table $$t0) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t1 $$t1 (i32.const 10) (i32.const 12) (i32.const 7)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:1425
invoke($16, `test`, []);

// ./test/core/table_copy.wast:1426
assert_trap(() => invoke($16, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1427
assert_trap(() => invoke($16, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1428
assert_return(() => invoke($16, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1429
assert_return(() => invoke($16, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1430
assert_return(() => invoke($16, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1431
assert_return(() => invoke($16, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1432
assert_trap(() => invoke($16, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1433
assert_trap(() => invoke($16, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1434
assert_trap(() => invoke($16, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1435
assert_trap(() => invoke($16, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1436
assert_return(() => invoke($16, `check_t0`, [10]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1437
assert_return(() => invoke($16, `check_t0`, [11]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1438
assert_return(() => invoke($16, `check_t0`, [12]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1439
assert_return(() => invoke($16, `check_t0`, [13]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1440
assert_return(() => invoke($16, `check_t0`, [14]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1441
assert_trap(() => invoke($16, `check_t0`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:1442
assert_trap(() => invoke($16, `check_t0`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1443
assert_trap(() => invoke($16, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1444
assert_trap(() => invoke($16, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1445
assert_trap(() => invoke($16, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1446
assert_trap(() => invoke($16, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1447
assert_trap(() => invoke($16, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1448
assert_trap(() => invoke($16, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1449
assert_trap(() => invoke($16, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1450
assert_trap(() => invoke($16, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1451
assert_trap(() => invoke($16, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1452
assert_trap(() => invoke($16, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1453
assert_trap(() => invoke($16, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1454
assert_trap(() => invoke($16, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1455
assert_trap(() => invoke($16, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1456
assert_trap(() => invoke($16, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1457
assert_trap(() => invoke($16, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1458
assert_trap(() => invoke($16, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1459
assert_return(() => invoke($16, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1460
assert_return(() => invoke($16, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1461
assert_return(() => invoke($16, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1462
assert_return(() => invoke($16, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1463
assert_trap(() => invoke($16, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1464
assert_trap(() => invoke($16, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1465
assert_trap(() => invoke($16, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1466
assert_trap(() => invoke($16, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1467
assert_return(() => invoke($16, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1468
assert_return(() => invoke($16, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1469
assert_return(() => invoke($16, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1470
assert_return(() => invoke($16, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1471
assert_return(() => invoke($16, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1472
assert_trap(() => invoke($16, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1473
assert_trap(() => invoke($16, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1474
assert_trap(() => invoke($16, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1475
assert_trap(() => invoke($16, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1476
assert_trap(() => invoke($16, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1477
assert_trap(() => invoke($16, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1478
assert_trap(() => invoke($16, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1479
assert_trap(() => invoke($16, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1480
assert_trap(() => invoke($16, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1481
assert_trap(() => invoke($16, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1482
assert_trap(() => invoke($16, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1483
assert_trap(() => invoke($16, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1484
assert_trap(() => invoke($16, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1485
assert_trap(() => invoke($16, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1487
let $17 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t0) (i32.const 3) func 1 3 1 4)
  (elem (table $$t0) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t1 $$t1 (i32.const 12) (i32.const 10) (i32.const 7)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:1517
invoke($17, `test`, []);

// ./test/core/table_copy.wast:1518
assert_trap(() => invoke($17, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1519
assert_trap(() => invoke($17, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1520
assert_return(() => invoke($17, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1521
assert_return(() => invoke($17, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1522
assert_return(() => invoke($17, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1523
assert_return(() => invoke($17, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1524
assert_trap(() => invoke($17, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1525
assert_trap(() => invoke($17, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1526
assert_trap(() => invoke($17, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1527
assert_trap(() => invoke($17, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1528
assert_trap(() => invoke($17, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1529
assert_trap(() => invoke($17, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:1530
assert_trap(() => invoke($17, `check_t0`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:1531
assert_trap(() => invoke($17, `check_t0`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:1532
assert_return(() => invoke($17, `check_t0`, [14]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1533
assert_return(() => invoke($17, `check_t0`, [15]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1534
assert_return(() => invoke($17, `check_t0`, [16]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1535
assert_return(() => invoke($17, `check_t0`, [17]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1536
assert_return(() => invoke($17, `check_t0`, [18]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1537
assert_trap(() => invoke($17, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1538
assert_trap(() => invoke($17, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1539
assert_trap(() => invoke($17, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1540
assert_trap(() => invoke($17, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1541
assert_trap(() => invoke($17, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1542
assert_trap(() => invoke($17, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1543
assert_trap(() => invoke($17, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1544
assert_trap(() => invoke($17, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1545
assert_trap(() => invoke($17, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1546
assert_trap(() => invoke($17, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1547
assert_trap(() => invoke($17, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1548
assert_trap(() => invoke($17, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1549
assert_trap(() => invoke($17, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1550
assert_trap(() => invoke($17, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1551
assert_return(() => invoke($17, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1552
assert_return(() => invoke($17, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1553
assert_return(() => invoke($17, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1554
assert_return(() => invoke($17, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1555
assert_trap(() => invoke($17, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1556
assert_trap(() => invoke($17, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1557
assert_trap(() => invoke($17, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1558
assert_trap(() => invoke($17, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1559
assert_return(() => invoke($17, `check_t1`, [11]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1560
assert_return(() => invoke($17, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1561
assert_return(() => invoke($17, `check_t1`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1562
assert_return(() => invoke($17, `check_t1`, [14]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1563
assert_return(() => invoke($17, `check_t1`, [15]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1564
assert_trap(() => invoke($17, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1565
assert_trap(() => invoke($17, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1566
assert_trap(() => invoke($17, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1567
assert_trap(() => invoke($17, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1568
assert_trap(() => invoke($17, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1569
assert_trap(() => invoke($17, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1570
assert_trap(() => invoke($17, `check_t1`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1571
assert_trap(() => invoke($17, `check_t1`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1572
assert_trap(() => invoke($17, `check_t1`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1573
assert_trap(() => invoke($17, `check_t1`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1574
assert_trap(() => invoke($17, `check_t1`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1575
assert_trap(() => invoke($17, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1576
assert_trap(() => invoke($17, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1577
assert_trap(() => invoke($17, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1579
let $18 = instantiate(`(module
  (type (func (result i32)))  ;; type #0
  (import "a" "ef0" (func (result i32)))    ;; index 0
  (import "a" "ef1" (func (result i32)))
  (import "a" "ef2" (func (result i32)))
  (import "a" "ef3" (func (result i32)))
  (import "a" "ef4" (func (result i32)))    ;; index 4
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t1) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t1) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (elem (table $$t0) (i32.const 3) func 1 3 1 4)
  (elem (table $$t0) (i32.const 11) func 6 3 2 5 7)
  (func (result i32) (i32.const 5))  ;; index 5
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))  ;; index 9
  (func (export "test")
    (table.copy $$t0 $$t1 (i32.const 10) (i32.const 0) (i32.const 20)))
  (func (export "check_t0") (param i32) (result i32)
    (call_indirect $$t1 (type 0) (local.get 0)))
  (func (export "check_t1") (param i32) (result i32)
    (call_indirect $$t0 (type 0) (local.get 0)))
)`);

// ./test/core/table_copy.wast:1609
invoke($18, `test`, []);

// ./test/core/table_copy.wast:1610
assert_trap(() => invoke($18, `check_t0`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1611
assert_trap(() => invoke($18, `check_t0`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1612
assert_return(() => invoke($18, `check_t0`, [2]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1613
assert_return(() => invoke($18, `check_t0`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1614
assert_return(() => invoke($18, `check_t0`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1615
assert_return(() => invoke($18, `check_t0`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1616
assert_trap(() => invoke($18, `check_t0`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:1617
assert_trap(() => invoke($18, `check_t0`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1618
assert_trap(() => invoke($18, `check_t0`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1619
assert_trap(() => invoke($18, `check_t0`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1620
assert_trap(() => invoke($18, `check_t0`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1621
assert_trap(() => invoke($18, `check_t0`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:1622
assert_return(() => invoke($18, `check_t0`, [12]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1623
assert_return(() => invoke($18, `check_t0`, [13]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1624
assert_return(() => invoke($18, `check_t0`, [14]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1625
assert_return(() => invoke($18, `check_t0`, [15]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1626
assert_return(() => invoke($18, `check_t0`, [16]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1627
assert_trap(() => invoke($18, `check_t0`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1628
assert_trap(() => invoke($18, `check_t0`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1629
assert_trap(() => invoke($18, `check_t0`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1630
assert_trap(() => invoke($18, `check_t0`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1631
assert_trap(() => invoke($18, `check_t0`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1632
assert_trap(() => invoke($18, `check_t0`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:1633
assert_trap(() => invoke($18, `check_t0`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:1634
assert_trap(() => invoke($18, `check_t0`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:1635
assert_trap(() => invoke($18, `check_t0`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:1636
assert_trap(() => invoke($18, `check_t0`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:1637
assert_trap(() => invoke($18, `check_t0`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1638
assert_trap(() => invoke($18, `check_t0`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1639
assert_trap(() => invoke($18, `check_t0`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1640
assert_trap(() => invoke($18, `check_t1`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:1641
assert_trap(() => invoke($18, `check_t1`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:1642
assert_trap(() => invoke($18, `check_t1`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:1643
assert_return(() => invoke($18, `check_t1`, [3]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1644
assert_return(() => invoke($18, `check_t1`, [4]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1645
assert_return(() => invoke($18, `check_t1`, [5]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1646
assert_return(() => invoke($18, `check_t1`, [6]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1647
assert_trap(() => invoke($18, `check_t1`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:1648
assert_trap(() => invoke($18, `check_t1`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:1649
assert_trap(() => invoke($18, `check_t1`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:1650
assert_trap(() => invoke($18, `check_t1`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:1651
assert_trap(() => invoke($18, `check_t1`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:1652
assert_return(() => invoke($18, `check_t1`, [12]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1653
assert_return(() => invoke($18, `check_t1`, [13]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1654
assert_return(() => invoke($18, `check_t1`, [14]), [value("i32", 4)]);

// ./test/core/table_copy.wast:1655
assert_return(() => invoke($18, `check_t1`, [15]), [value("i32", 1)]);

// ./test/core/table_copy.wast:1656
assert_trap(() => invoke($18, `check_t1`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:1657
assert_trap(() => invoke($18, `check_t1`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:1658
assert_trap(() => invoke($18, `check_t1`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:1659
assert_trap(() => invoke($18, `check_t1`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:1660
assert_trap(() => invoke($18, `check_t1`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:1661
assert_trap(() => invoke($18, `check_t1`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:1662
assert_return(() => invoke($18, `check_t1`, [22]), [value("i32", 7)]);

// ./test/core/table_copy.wast:1663
assert_return(() => invoke($18, `check_t1`, [23]), [value("i32", 5)]);

// ./test/core/table_copy.wast:1664
assert_return(() => invoke($18, `check_t1`, [24]), [value("i32", 2)]);

// ./test/core/table_copy.wast:1665
assert_return(() => invoke($18, `check_t1`, [25]), [value("i32", 3)]);

// ./test/core/table_copy.wast:1666
assert_return(() => invoke($18, `check_t1`, [26]), [value("i32", 6)]);

// ./test/core/table_copy.wast:1667
assert_trap(() => invoke($18, `check_t1`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:1668
assert_trap(() => invoke($18, `check_t1`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:1669
assert_trap(() => invoke($18, `check_t1`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:1671
let $19 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 28) (i32.const 1) (i32.const 3))
    ))`);

// ./test/core/table_copy.wast:1694
assert_trap(() => invoke($19, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:1696
let $20 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 0xFFFFFFFE) (i32.const 1) (i32.const 2))
    ))`);

// ./test/core/table_copy.wast:1719
assert_trap(() => invoke($20, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:1721
let $21 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 15) (i32.const 25) (i32.const 6))
    ))`);

// ./test/core/table_copy.wast:1744
assert_trap(() => invoke($21, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:1746
let $22 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 15) (i32.const 0xFFFFFFFE) (i32.const 2))
    ))`);

// ./test/core/table_copy.wast:1769
assert_trap(() => invoke($22, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:1771
let $23 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 15) (i32.const 25) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:1794
invoke($23, `test`, []);

// ./test/core/table_copy.wast:1796
let $24 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 30) (i32.const 15) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:1819
invoke($24, `test`, []);

// ./test/core/table_copy.wast:1821
let $25 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 31) (i32.const 15) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:1844
assert_trap(() => invoke($25, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:1846
let $26 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 15) (i32.const 30) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:1869
invoke($26, `test`, []);

// ./test/core/table_copy.wast:1871
let $27 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 15) (i32.const 31) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:1894
assert_trap(() => invoke($27, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:1896
let $28 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 30) (i32.const 30) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:1919
invoke($28, `test`, []);

// ./test/core/table_copy.wast:1921
let $29 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i32.const 31) (i32.const 31) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:1944
assert_trap(() => invoke($29, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:1946
let $30 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i32.const 28) (i32.const 1) (i32.const 3))
    ))`);

// ./test/core/table_copy.wast:1969
assert_trap(() => invoke($30, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:1971
let $31 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i32.const 0xFFFFFFFE) (i32.const 1) (i32.const 2))
    ))`);

// ./test/core/table_copy.wast:1994
assert_trap(() => invoke($31, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:1996
let $32 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i32.const 15) (i32.const 25) (i32.const 6))
    ))`);

// ./test/core/table_copy.wast:2019
assert_trap(() => invoke($32, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2021
let $33 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i32.const 15) (i32.const 0xFFFFFFFE) (i32.const 2))
    ))`);

// ./test/core/table_copy.wast:2044
assert_trap(() => invoke($33, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2046
let $34 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i32.const 15) (i32.const 25) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:2069
invoke($34, `test`, []);

// ./test/core/table_copy.wast:2071
let $35 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i32.const 30) (i32.const 15) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:2094
invoke($35, `test`, []);

// ./test/core/table_copy.wast:2096
let $36 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i32.const 31) (i32.const 15) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:2119
assert_trap(() => invoke($36, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2121
let $37 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i32.const 15) (i32.const 30) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:2144
invoke($37, `test`, []);

// ./test/core/table_copy.wast:2146
let $38 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i32.const 15) (i32.const 31) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:2169
assert_trap(() => invoke($38, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2171
let $39 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i32.const 30) (i32.const 30) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:2194
invoke($39, `test`, []);

// ./test/core/table_copy.wast:2196
let $40 = instantiate(`(module
  (table $$t0 30 30 funcref)
  (table $$t1 30 30 funcref)
  (elem (table $$t0) (i32.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i32.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i32.const 31) (i32.const 31) (i32.const 0))
    ))`);

// ./test/core/table_copy.wast:2219
assert_trap(() => invoke($40, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2221
let $41 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i64.const 28) (i64.const 1) (i64.const 3))
    ))`);

// ./test/core/table_copy.wast:2244
assert_trap(() => invoke($41, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2246
let $42 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i64.const 0xFFFFFFFE) (i64.const 1) (i64.const 2))
    ))`);

// ./test/core/table_copy.wast:2269
assert_trap(() => invoke($42, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2271
let $43 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i64.const 15) (i64.const 25) (i64.const 6))
    ))`);

// ./test/core/table_copy.wast:2294
assert_trap(() => invoke($43, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2296
let $44 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i64.const 15) (i64.const 0xFFFFFFFE) (i64.const 2))
    ))`);

// ./test/core/table_copy.wast:2319
assert_trap(() => invoke($44, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2321
let $45 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i64.const 15) (i64.const 25) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2344
invoke($45, `test`, []);

// ./test/core/table_copy.wast:2346
let $46 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i64.const 30) (i64.const 15) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2369
invoke($46, `test`, []);

// ./test/core/table_copy.wast:2371
let $47 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i64.const 31) (i64.const 15) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2394
assert_trap(() => invoke($47, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2396
let $48 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i64.const 15) (i64.const 30) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2419
invoke($48, `test`, []);

// ./test/core/table_copy.wast:2421
let $49 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i64.const 15) (i64.const 31) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2444
assert_trap(() => invoke($49, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2446
let $50 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i64.const 30) (i64.const 30) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2469
invoke($50, `test`, []);

// ./test/core/table_copy.wast:2471
let $51 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t0 $$t0 (i64.const 31) (i64.const 31) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2494
assert_trap(() => invoke($51, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2496
let $52 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i64.const 28) (i64.const 1) (i64.const 3))
    ))`);

// ./test/core/table_copy.wast:2519
assert_trap(() => invoke($52, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2521
let $53 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i64.const 0xFFFFFFFE) (i64.const 1) (i64.const 2))
    ))`);

// ./test/core/table_copy.wast:2544
assert_trap(() => invoke($53, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2546
let $54 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i64.const 15) (i64.const 25) (i64.const 6))
    ))`);

// ./test/core/table_copy.wast:2569
assert_trap(() => invoke($54, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2571
let $55 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i64.const 15) (i64.const 0xFFFFFFFE) (i64.const 2))
    ))`);

// ./test/core/table_copy.wast:2594
assert_trap(() => invoke($55, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2596
let $56 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i64.const 15) (i64.const 25) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2619
invoke($56, `test`, []);

// ./test/core/table_copy.wast:2621
let $57 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i64.const 30) (i64.const 15) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2644
invoke($57, `test`, []);

// ./test/core/table_copy.wast:2646
let $58 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i64.const 31) (i64.const 15) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2669
assert_trap(() => invoke($58, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2671
let $59 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i64.const 15) (i64.const 30) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2694
invoke($59, `test`, []);

// ./test/core/table_copy.wast:2696
let $60 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i64.const 15) (i64.const 31) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2719
assert_trap(() => invoke($60, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2721
let $61 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i64.const 30) (i64.const 30) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2744
invoke($61, `test`, []);

// ./test/core/table_copy.wast:2746
let $62 = instantiate(`(module
  (table $$t0 i64 30 30 funcref)
  (table $$t1 i64 30 30 funcref)
  (elem (table $$t0) (i64.const 2) func 3 1 4 1)
  (elem funcref
    (ref.func 2) (ref.func 7) (ref.func 1) (ref.func 8))
  (elem (table $$t0) (i64.const 12) func 7 5 2 3 6)
  (elem funcref
    (ref.func 5) (ref.func 9) (ref.func 2) (ref.func 7) (ref.func 6))
  (func (result i32) (i32.const 0))
  (func (result i32) (i32.const 1))
  (func (result i32) (i32.const 2))
  (func (result i32) (i32.const 3))
  (func (result i32) (i32.const 4))
  (func (result i32) (i32.const 5))
  (func (result i32) (i32.const 6))
  (func (result i32) (i32.const 7))
  (func (result i32) (i32.const 8))
  (func (result i32) (i32.const 9))
  (func (export "test")
    (table.copy $$t1 $$t0 (i64.const 31) (i64.const 31) (i64.const 0))
    ))`);

// ./test/core/table_copy.wast:2769
assert_trap(() => invoke($62, `test`, []), `out of bounds table access`);

// ./test/core/table_copy.wast:2771
let $63 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 0)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7)
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:2797
assert_trap(() => invoke($63, `run`, [24, 0, 16]), `out of bounds table access`);

// ./test/core/table_copy.wast:2799
assert_return(() => invoke($63, `test`, [0]), [value("i32", 0)]);

// ./test/core/table_copy.wast:2800
assert_return(() => invoke($63, `test`, [1]), [value("i32", 1)]);

// ./test/core/table_copy.wast:2801
assert_return(() => invoke($63, `test`, [2]), [value("i32", 2)]);

// ./test/core/table_copy.wast:2802
assert_return(() => invoke($63, `test`, [3]), [value("i32", 3)]);

// ./test/core/table_copy.wast:2803
assert_return(() => invoke($63, `test`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:2804
assert_return(() => invoke($63, `test`, [5]), [value("i32", 5)]);

// ./test/core/table_copy.wast:2805
assert_return(() => invoke($63, `test`, [6]), [value("i32", 6)]);

// ./test/core/table_copy.wast:2806
assert_return(() => invoke($63, `test`, [7]), [value("i32", 7)]);

// ./test/core/table_copy.wast:2807
assert_trap(() => invoke($63, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:2808
assert_trap(() => invoke($63, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:2809
assert_trap(() => invoke($63, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:2810
assert_trap(() => invoke($63, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:2811
assert_trap(() => invoke($63, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:2812
assert_trap(() => invoke($63, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:2813
assert_trap(() => invoke($63, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:2814
assert_trap(() => invoke($63, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:2815
assert_trap(() => invoke($63, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:2816
assert_trap(() => invoke($63, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:2817
assert_trap(() => invoke($63, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:2818
assert_trap(() => invoke($63, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:2819
assert_trap(() => invoke($63, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:2820
assert_trap(() => invoke($63, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:2821
assert_trap(() => invoke($63, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:2822
assert_trap(() => invoke($63, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:2823
assert_trap(() => invoke($63, `test`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:2824
assert_trap(() => invoke($63, `test`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:2825
assert_trap(() => invoke($63, `test`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:2826
assert_trap(() => invoke($63, `test`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:2827
assert_trap(() => invoke($63, `test`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:2828
assert_trap(() => invoke($63, `test`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:2829
assert_trap(() => invoke($63, `test`, [30]), `uninitialized element`);

// ./test/core/table_copy.wast:2830
assert_trap(() => invoke($63, `test`, [31]), `uninitialized element`);

// ./test/core/table_copy.wast:2832
let $64 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 0)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7 $$f8)
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:2858
assert_trap(() => invoke($64, `run`, [23, 0, 15]), `out of bounds table access`);

// ./test/core/table_copy.wast:2860
assert_return(() => invoke($64, `test`, [0]), [value("i32", 0)]);

// ./test/core/table_copy.wast:2861
assert_return(() => invoke($64, `test`, [1]), [value("i32", 1)]);

// ./test/core/table_copy.wast:2862
assert_return(() => invoke($64, `test`, [2]), [value("i32", 2)]);

// ./test/core/table_copy.wast:2863
assert_return(() => invoke($64, `test`, [3]), [value("i32", 3)]);

// ./test/core/table_copy.wast:2864
assert_return(() => invoke($64, `test`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:2865
assert_return(() => invoke($64, `test`, [5]), [value("i32", 5)]);

// ./test/core/table_copy.wast:2866
assert_return(() => invoke($64, `test`, [6]), [value("i32", 6)]);

// ./test/core/table_copy.wast:2867
assert_return(() => invoke($64, `test`, [7]), [value("i32", 7)]);

// ./test/core/table_copy.wast:2868
assert_return(() => invoke($64, `test`, [8]), [value("i32", 8)]);

// ./test/core/table_copy.wast:2869
assert_trap(() => invoke($64, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:2870
assert_trap(() => invoke($64, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:2871
assert_trap(() => invoke($64, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:2872
assert_trap(() => invoke($64, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:2873
assert_trap(() => invoke($64, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:2874
assert_trap(() => invoke($64, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:2875
assert_trap(() => invoke($64, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:2876
assert_trap(() => invoke($64, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:2877
assert_trap(() => invoke($64, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:2878
assert_trap(() => invoke($64, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:2879
assert_trap(() => invoke($64, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:2880
assert_trap(() => invoke($64, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:2881
assert_trap(() => invoke($64, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:2882
assert_trap(() => invoke($64, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:2883
assert_trap(() => invoke($64, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:2884
assert_trap(() => invoke($64, `test`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:2885
assert_trap(() => invoke($64, `test`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:2886
assert_trap(() => invoke($64, `test`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:2887
assert_trap(() => invoke($64, `test`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:2888
assert_trap(() => invoke($64, `test`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:2889
assert_trap(() => invoke($64, `test`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:2890
assert_trap(() => invoke($64, `test`, [30]), `uninitialized element`);

// ./test/core/table_copy.wast:2891
assert_trap(() => invoke($64, `test`, [31]), `uninitialized element`);

// ./test/core/table_copy.wast:2893
let $65 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 24)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7)
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:2919
assert_trap(() => invoke($65, `run`, [0, 24, 16]), `out of bounds table access`);

// ./test/core/table_copy.wast:2921
assert_trap(() => invoke($65, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:2922
assert_trap(() => invoke($65, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:2923
assert_trap(() => invoke($65, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:2924
assert_trap(() => invoke($65, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:2925
assert_trap(() => invoke($65, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:2926
assert_trap(() => invoke($65, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:2927
assert_trap(() => invoke($65, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:2928
assert_trap(() => invoke($65, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:2929
assert_trap(() => invoke($65, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:2930
assert_trap(() => invoke($65, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:2931
assert_trap(() => invoke($65, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:2932
assert_trap(() => invoke($65, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:2933
assert_trap(() => invoke($65, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:2934
assert_trap(() => invoke($65, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:2935
assert_trap(() => invoke($65, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:2936
assert_trap(() => invoke($65, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:2937
assert_trap(() => invoke($65, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:2938
assert_trap(() => invoke($65, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:2939
assert_trap(() => invoke($65, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:2940
assert_trap(() => invoke($65, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:2941
assert_trap(() => invoke($65, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:2942
assert_trap(() => invoke($65, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:2943
assert_trap(() => invoke($65, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:2944
assert_trap(() => invoke($65, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:2945
assert_return(() => invoke($65, `test`, [24]), [value("i32", 0)]);

// ./test/core/table_copy.wast:2946
assert_return(() => invoke($65, `test`, [25]), [value("i32", 1)]);

// ./test/core/table_copy.wast:2947
assert_return(() => invoke($65, `test`, [26]), [value("i32", 2)]);

// ./test/core/table_copy.wast:2948
assert_return(() => invoke($65, `test`, [27]), [value("i32", 3)]);

// ./test/core/table_copy.wast:2949
assert_return(() => invoke($65, `test`, [28]), [value("i32", 4)]);

// ./test/core/table_copy.wast:2950
assert_return(() => invoke($65, `test`, [29]), [value("i32", 5)]);

// ./test/core/table_copy.wast:2951
assert_return(() => invoke($65, `test`, [30]), [value("i32", 6)]);

// ./test/core/table_copy.wast:2952
assert_return(() => invoke($65, `test`, [31]), [value("i32", 7)]);

// ./test/core/table_copy.wast:2954
let $66 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 23)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7 $$f8)
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:2980
assert_trap(() => invoke($66, `run`, [0, 23, 15]), `out of bounds table access`);

// ./test/core/table_copy.wast:2982
assert_trap(() => invoke($66, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:2983
assert_trap(() => invoke($66, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:2984
assert_trap(() => invoke($66, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:2985
assert_trap(() => invoke($66, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:2986
assert_trap(() => invoke($66, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:2987
assert_trap(() => invoke($66, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:2988
assert_trap(() => invoke($66, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:2989
assert_trap(() => invoke($66, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:2990
assert_trap(() => invoke($66, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:2991
assert_trap(() => invoke($66, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:2992
assert_trap(() => invoke($66, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:2993
assert_trap(() => invoke($66, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:2994
assert_trap(() => invoke($66, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:2995
assert_trap(() => invoke($66, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:2996
assert_trap(() => invoke($66, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:2997
assert_trap(() => invoke($66, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:2998
assert_trap(() => invoke($66, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:2999
assert_trap(() => invoke($66, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:3000
assert_trap(() => invoke($66, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:3001
assert_trap(() => invoke($66, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:3002
assert_trap(() => invoke($66, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:3003
assert_trap(() => invoke($66, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:3004
assert_trap(() => invoke($66, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:3005
assert_return(() => invoke($66, `test`, [23]), [value("i32", 0)]);

// ./test/core/table_copy.wast:3006
assert_return(() => invoke($66, `test`, [24]), [value("i32", 1)]);

// ./test/core/table_copy.wast:3007
assert_return(() => invoke($66, `test`, [25]), [value("i32", 2)]);

// ./test/core/table_copy.wast:3008
assert_return(() => invoke($66, `test`, [26]), [value("i32", 3)]);

// ./test/core/table_copy.wast:3009
assert_return(() => invoke($66, `test`, [27]), [value("i32", 4)]);

// ./test/core/table_copy.wast:3010
assert_return(() => invoke($66, `test`, [28]), [value("i32", 5)]);

// ./test/core/table_copy.wast:3011
assert_return(() => invoke($66, `test`, [29]), [value("i32", 6)]);

// ./test/core/table_copy.wast:3012
assert_return(() => invoke($66, `test`, [30]), [value("i32", 7)]);

// ./test/core/table_copy.wast:3013
assert_return(() => invoke($66, `test`, [31]), [value("i32", 8)]);

// ./test/core/table_copy.wast:3015
let $67 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 11)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7)
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:3041
assert_trap(() => invoke($67, `run`, [24, 11, 16]), `out of bounds table access`);

// ./test/core/table_copy.wast:3043
assert_trap(() => invoke($67, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:3044
assert_trap(() => invoke($67, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:3045
assert_trap(() => invoke($67, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:3046
assert_trap(() => invoke($67, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:3047
assert_trap(() => invoke($67, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:3048
assert_trap(() => invoke($67, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:3049
assert_trap(() => invoke($67, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:3050
assert_trap(() => invoke($67, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:3051
assert_trap(() => invoke($67, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:3052
assert_trap(() => invoke($67, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:3053
assert_trap(() => invoke($67, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:3054
assert_return(() => invoke($67, `test`, [11]), [value("i32", 0)]);

// ./test/core/table_copy.wast:3055
assert_return(() => invoke($67, `test`, [12]), [value("i32", 1)]);

// ./test/core/table_copy.wast:3056
assert_return(() => invoke($67, `test`, [13]), [value("i32", 2)]);

// ./test/core/table_copy.wast:3057
assert_return(() => invoke($67, `test`, [14]), [value("i32", 3)]);

// ./test/core/table_copy.wast:3058
assert_return(() => invoke($67, `test`, [15]), [value("i32", 4)]);

// ./test/core/table_copy.wast:3059
assert_return(() => invoke($67, `test`, [16]), [value("i32", 5)]);

// ./test/core/table_copy.wast:3060
assert_return(() => invoke($67, `test`, [17]), [value("i32", 6)]);

// ./test/core/table_copy.wast:3061
assert_return(() => invoke($67, `test`, [18]), [value("i32", 7)]);

// ./test/core/table_copy.wast:3062
assert_trap(() => invoke($67, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:3063
assert_trap(() => invoke($67, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:3064
assert_trap(() => invoke($67, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:3065
assert_trap(() => invoke($67, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:3066
assert_trap(() => invoke($67, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:3067
assert_trap(() => invoke($67, `test`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:3068
assert_trap(() => invoke($67, `test`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:3069
assert_trap(() => invoke($67, `test`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:3070
assert_trap(() => invoke($67, `test`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:3071
assert_trap(() => invoke($67, `test`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:3072
assert_trap(() => invoke($67, `test`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:3073
assert_trap(() => invoke($67, `test`, [30]), `uninitialized element`);

// ./test/core/table_copy.wast:3074
assert_trap(() => invoke($67, `test`, [31]), `uninitialized element`);

// ./test/core/table_copy.wast:3076
let $68 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 24)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7)
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:3102
assert_trap(() => invoke($68, `run`, [11, 24, 16]), `out of bounds table access`);

// ./test/core/table_copy.wast:3104
assert_trap(() => invoke($68, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:3105
assert_trap(() => invoke($68, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:3106
assert_trap(() => invoke($68, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:3107
assert_trap(() => invoke($68, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:3108
assert_trap(() => invoke($68, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:3109
assert_trap(() => invoke($68, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:3110
assert_trap(() => invoke($68, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:3111
assert_trap(() => invoke($68, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:3112
assert_trap(() => invoke($68, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:3113
assert_trap(() => invoke($68, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:3114
assert_trap(() => invoke($68, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:3115
assert_trap(() => invoke($68, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:3116
assert_trap(() => invoke($68, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:3117
assert_trap(() => invoke($68, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:3118
assert_trap(() => invoke($68, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:3119
assert_trap(() => invoke($68, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:3120
assert_trap(() => invoke($68, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:3121
assert_trap(() => invoke($68, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:3122
assert_trap(() => invoke($68, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:3123
assert_trap(() => invoke($68, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:3124
assert_trap(() => invoke($68, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:3125
assert_trap(() => invoke($68, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:3126
assert_trap(() => invoke($68, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:3127
assert_trap(() => invoke($68, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:3128
assert_return(() => invoke($68, `test`, [24]), [value("i32", 0)]);

// ./test/core/table_copy.wast:3129
assert_return(() => invoke($68, `test`, [25]), [value("i32", 1)]);

// ./test/core/table_copy.wast:3130
assert_return(() => invoke($68, `test`, [26]), [value("i32", 2)]);

// ./test/core/table_copy.wast:3131
assert_return(() => invoke($68, `test`, [27]), [value("i32", 3)]);

// ./test/core/table_copy.wast:3132
assert_return(() => invoke($68, `test`, [28]), [value("i32", 4)]);

// ./test/core/table_copy.wast:3133
assert_return(() => invoke($68, `test`, [29]), [value("i32", 5)]);

// ./test/core/table_copy.wast:3134
assert_return(() => invoke($68, `test`, [30]), [value("i32", 6)]);

// ./test/core/table_copy.wast:3135
assert_return(() => invoke($68, `test`, [31]), [value("i32", 7)]);

// ./test/core/table_copy.wast:3137
let $69 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 21)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7)
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:3163
assert_trap(() => invoke($69, `run`, [24, 21, 16]), `out of bounds table access`);

// ./test/core/table_copy.wast:3165
assert_trap(() => invoke($69, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:3166
assert_trap(() => invoke($69, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:3167
assert_trap(() => invoke($69, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:3168
assert_trap(() => invoke($69, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:3169
assert_trap(() => invoke($69, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:3170
assert_trap(() => invoke($69, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:3171
assert_trap(() => invoke($69, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:3172
assert_trap(() => invoke($69, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:3173
assert_trap(() => invoke($69, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:3174
assert_trap(() => invoke($69, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:3175
assert_trap(() => invoke($69, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:3176
assert_trap(() => invoke($69, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:3177
assert_trap(() => invoke($69, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:3178
assert_trap(() => invoke($69, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:3179
assert_trap(() => invoke($69, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:3180
assert_trap(() => invoke($69, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:3181
assert_trap(() => invoke($69, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:3182
assert_trap(() => invoke($69, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:3183
assert_trap(() => invoke($69, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:3184
assert_trap(() => invoke($69, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:3185
assert_trap(() => invoke($69, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:3186
assert_return(() => invoke($69, `test`, [21]), [value("i32", 0)]);

// ./test/core/table_copy.wast:3187
assert_return(() => invoke($69, `test`, [22]), [value("i32", 1)]);

// ./test/core/table_copy.wast:3188
assert_return(() => invoke($69, `test`, [23]), [value("i32", 2)]);

// ./test/core/table_copy.wast:3189
assert_return(() => invoke($69, `test`, [24]), [value("i32", 3)]);

// ./test/core/table_copy.wast:3190
assert_return(() => invoke($69, `test`, [25]), [value("i32", 4)]);

// ./test/core/table_copy.wast:3191
assert_return(() => invoke($69, `test`, [26]), [value("i32", 5)]);

// ./test/core/table_copy.wast:3192
assert_return(() => invoke($69, `test`, [27]), [value("i32", 6)]);

// ./test/core/table_copy.wast:3193
assert_return(() => invoke($69, `test`, [28]), [value("i32", 7)]);

// ./test/core/table_copy.wast:3194
assert_trap(() => invoke($69, `test`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:3195
assert_trap(() => invoke($69, `test`, [30]), `uninitialized element`);

// ./test/core/table_copy.wast:3196
assert_trap(() => invoke($69, `test`, [31]), `uninitialized element`);

// ./test/core/table_copy.wast:3198
let $70 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 24)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7)
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:3224
assert_trap(() => invoke($70, `run`, [21, 24, 16]), `out of bounds table access`);

// ./test/core/table_copy.wast:3226
assert_trap(() => invoke($70, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:3227
assert_trap(() => invoke($70, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:3228
assert_trap(() => invoke($70, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:3229
assert_trap(() => invoke($70, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:3230
assert_trap(() => invoke($70, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:3231
assert_trap(() => invoke($70, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:3232
assert_trap(() => invoke($70, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:3233
assert_trap(() => invoke($70, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:3234
assert_trap(() => invoke($70, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:3235
assert_trap(() => invoke($70, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:3236
assert_trap(() => invoke($70, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:3237
assert_trap(() => invoke($70, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:3238
assert_trap(() => invoke($70, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:3239
assert_trap(() => invoke($70, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:3240
assert_trap(() => invoke($70, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:3241
assert_trap(() => invoke($70, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:3242
assert_trap(() => invoke($70, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:3243
assert_trap(() => invoke($70, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:3244
assert_trap(() => invoke($70, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:3245
assert_trap(() => invoke($70, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:3246
assert_trap(() => invoke($70, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:3247
assert_trap(() => invoke($70, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:3248
assert_trap(() => invoke($70, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:3249
assert_trap(() => invoke($70, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:3250
assert_return(() => invoke($70, `test`, [24]), [value("i32", 0)]);

// ./test/core/table_copy.wast:3251
assert_return(() => invoke($70, `test`, [25]), [value("i32", 1)]);

// ./test/core/table_copy.wast:3252
assert_return(() => invoke($70, `test`, [26]), [value("i32", 2)]);

// ./test/core/table_copy.wast:3253
assert_return(() => invoke($70, `test`, [27]), [value("i32", 3)]);

// ./test/core/table_copy.wast:3254
assert_return(() => invoke($70, `test`, [28]), [value("i32", 4)]);

// ./test/core/table_copy.wast:3255
assert_return(() => invoke($70, `test`, [29]), [value("i32", 5)]);

// ./test/core/table_copy.wast:3256
assert_return(() => invoke($70, `test`, [30]), [value("i32", 6)]);

// ./test/core/table_copy.wast:3257
assert_return(() => invoke($70, `test`, [31]), [value("i32", 7)]);

// ./test/core/table_copy.wast:3259
let $71 = instantiate(`(module
  (type (func (result i32)))
  (table 32 64 funcref)
  (elem (i32.const 21)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7 $$f8 $$f9 $$f10)
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:3285
assert_trap(() => invoke($71, `run`, [21, 21, 16]), `out of bounds table access`);

// ./test/core/table_copy.wast:3287
assert_trap(() => invoke($71, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:3288
assert_trap(() => invoke($71, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:3289
assert_trap(() => invoke($71, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:3290
assert_trap(() => invoke($71, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:3291
assert_trap(() => invoke($71, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:3292
assert_trap(() => invoke($71, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:3293
assert_trap(() => invoke($71, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:3294
assert_trap(() => invoke($71, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:3295
assert_trap(() => invoke($71, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:3296
assert_trap(() => invoke($71, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:3297
assert_trap(() => invoke($71, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:3298
assert_trap(() => invoke($71, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:3299
assert_trap(() => invoke($71, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:3300
assert_trap(() => invoke($71, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:3301
assert_trap(() => invoke($71, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:3302
assert_trap(() => invoke($71, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:3303
assert_trap(() => invoke($71, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:3304
assert_trap(() => invoke($71, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:3305
assert_trap(() => invoke($71, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:3306
assert_trap(() => invoke($71, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:3307
assert_trap(() => invoke($71, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:3308
assert_return(() => invoke($71, `test`, [21]), [value("i32", 0)]);

// ./test/core/table_copy.wast:3309
assert_return(() => invoke($71, `test`, [22]), [value("i32", 1)]);

// ./test/core/table_copy.wast:3310
assert_return(() => invoke($71, `test`, [23]), [value("i32", 2)]);

// ./test/core/table_copy.wast:3311
assert_return(() => invoke($71, `test`, [24]), [value("i32", 3)]);

// ./test/core/table_copy.wast:3312
assert_return(() => invoke($71, `test`, [25]), [value("i32", 4)]);

// ./test/core/table_copy.wast:3313
assert_return(() => invoke($71, `test`, [26]), [value("i32", 5)]);

// ./test/core/table_copy.wast:3314
assert_return(() => invoke($71, `test`, [27]), [value("i32", 6)]);

// ./test/core/table_copy.wast:3315
assert_return(() => invoke($71, `test`, [28]), [value("i32", 7)]);

// ./test/core/table_copy.wast:3316
assert_return(() => invoke($71, `test`, [29]), [value("i32", 8)]);

// ./test/core/table_copy.wast:3317
assert_return(() => invoke($71, `test`, [30]), [value("i32", 9)]);

// ./test/core/table_copy.wast:3318
assert_return(() => invoke($71, `test`, [31]), [value("i32", 10)]);

// ./test/core/table_copy.wast:3320
let $72 = instantiate(`(module
  (type (func (result i32)))
  (table 128 128 funcref)
  (elem (i32.const 112)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7 $$f8 $$f9 $$f10 $$f11 $$f12 $$f13 $$f14 $$f15)
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:3346
assert_trap(() => invoke($72, `run`, [0, 112, -32]), `out of bounds table access`);

// ./test/core/table_copy.wast:3348
assert_trap(() => invoke($72, `test`, [0]), `uninitialized element`);

// ./test/core/table_copy.wast:3349
assert_trap(() => invoke($72, `test`, [1]), `uninitialized element`);

// ./test/core/table_copy.wast:3350
assert_trap(() => invoke($72, `test`, [2]), `uninitialized element`);

// ./test/core/table_copy.wast:3351
assert_trap(() => invoke($72, `test`, [3]), `uninitialized element`);

// ./test/core/table_copy.wast:3352
assert_trap(() => invoke($72, `test`, [4]), `uninitialized element`);

// ./test/core/table_copy.wast:3353
assert_trap(() => invoke($72, `test`, [5]), `uninitialized element`);

// ./test/core/table_copy.wast:3354
assert_trap(() => invoke($72, `test`, [6]), `uninitialized element`);

// ./test/core/table_copy.wast:3355
assert_trap(() => invoke($72, `test`, [7]), `uninitialized element`);

// ./test/core/table_copy.wast:3356
assert_trap(() => invoke($72, `test`, [8]), `uninitialized element`);

// ./test/core/table_copy.wast:3357
assert_trap(() => invoke($72, `test`, [9]), `uninitialized element`);

// ./test/core/table_copy.wast:3358
assert_trap(() => invoke($72, `test`, [10]), `uninitialized element`);

// ./test/core/table_copy.wast:3359
assert_trap(() => invoke($72, `test`, [11]), `uninitialized element`);

// ./test/core/table_copy.wast:3360
assert_trap(() => invoke($72, `test`, [12]), `uninitialized element`);

// ./test/core/table_copy.wast:3361
assert_trap(() => invoke($72, `test`, [13]), `uninitialized element`);

// ./test/core/table_copy.wast:3362
assert_trap(() => invoke($72, `test`, [14]), `uninitialized element`);

// ./test/core/table_copy.wast:3363
assert_trap(() => invoke($72, `test`, [15]), `uninitialized element`);

// ./test/core/table_copy.wast:3364
assert_trap(() => invoke($72, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:3365
assert_trap(() => invoke($72, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:3366
assert_trap(() => invoke($72, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:3367
assert_trap(() => invoke($72, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:3368
assert_trap(() => invoke($72, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:3369
assert_trap(() => invoke($72, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:3370
assert_trap(() => invoke($72, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:3371
assert_trap(() => invoke($72, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:3372
assert_trap(() => invoke($72, `test`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:3373
assert_trap(() => invoke($72, `test`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:3374
assert_trap(() => invoke($72, `test`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:3375
assert_trap(() => invoke($72, `test`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:3376
assert_trap(() => invoke($72, `test`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:3377
assert_trap(() => invoke($72, `test`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:3378
assert_trap(() => invoke($72, `test`, [30]), `uninitialized element`);

// ./test/core/table_copy.wast:3379
assert_trap(() => invoke($72, `test`, [31]), `uninitialized element`);

// ./test/core/table_copy.wast:3380
assert_trap(() => invoke($72, `test`, [32]), `uninitialized element`);

// ./test/core/table_copy.wast:3381
assert_trap(() => invoke($72, `test`, [33]), `uninitialized element`);

// ./test/core/table_copy.wast:3382
assert_trap(() => invoke($72, `test`, [34]), `uninitialized element`);

// ./test/core/table_copy.wast:3383
assert_trap(() => invoke($72, `test`, [35]), `uninitialized element`);

// ./test/core/table_copy.wast:3384
assert_trap(() => invoke($72, `test`, [36]), `uninitialized element`);

// ./test/core/table_copy.wast:3385
assert_trap(() => invoke($72, `test`, [37]), `uninitialized element`);

// ./test/core/table_copy.wast:3386
assert_trap(() => invoke($72, `test`, [38]), `uninitialized element`);

// ./test/core/table_copy.wast:3387
assert_trap(() => invoke($72, `test`, [39]), `uninitialized element`);

// ./test/core/table_copy.wast:3388
assert_trap(() => invoke($72, `test`, [40]), `uninitialized element`);

// ./test/core/table_copy.wast:3389
assert_trap(() => invoke($72, `test`, [41]), `uninitialized element`);

// ./test/core/table_copy.wast:3390
assert_trap(() => invoke($72, `test`, [42]), `uninitialized element`);

// ./test/core/table_copy.wast:3391
assert_trap(() => invoke($72, `test`, [43]), `uninitialized element`);

// ./test/core/table_copy.wast:3392
assert_trap(() => invoke($72, `test`, [44]), `uninitialized element`);

// ./test/core/table_copy.wast:3393
assert_trap(() => invoke($72, `test`, [45]), `uninitialized element`);

// ./test/core/table_copy.wast:3394
assert_trap(() => invoke($72, `test`, [46]), `uninitialized element`);

// ./test/core/table_copy.wast:3395
assert_trap(() => invoke($72, `test`, [47]), `uninitialized element`);

// ./test/core/table_copy.wast:3396
assert_trap(() => invoke($72, `test`, [48]), `uninitialized element`);

// ./test/core/table_copy.wast:3397
assert_trap(() => invoke($72, `test`, [49]), `uninitialized element`);

// ./test/core/table_copy.wast:3398
assert_trap(() => invoke($72, `test`, [50]), `uninitialized element`);

// ./test/core/table_copy.wast:3399
assert_trap(() => invoke($72, `test`, [51]), `uninitialized element`);

// ./test/core/table_copy.wast:3400
assert_trap(() => invoke($72, `test`, [52]), `uninitialized element`);

// ./test/core/table_copy.wast:3401
assert_trap(() => invoke($72, `test`, [53]), `uninitialized element`);

// ./test/core/table_copy.wast:3402
assert_trap(() => invoke($72, `test`, [54]), `uninitialized element`);

// ./test/core/table_copy.wast:3403
assert_trap(() => invoke($72, `test`, [55]), `uninitialized element`);

// ./test/core/table_copy.wast:3404
assert_trap(() => invoke($72, `test`, [56]), `uninitialized element`);

// ./test/core/table_copy.wast:3405
assert_trap(() => invoke($72, `test`, [57]), `uninitialized element`);

// ./test/core/table_copy.wast:3406
assert_trap(() => invoke($72, `test`, [58]), `uninitialized element`);

// ./test/core/table_copy.wast:3407
assert_trap(() => invoke($72, `test`, [59]), `uninitialized element`);

// ./test/core/table_copy.wast:3408
assert_trap(() => invoke($72, `test`, [60]), `uninitialized element`);

// ./test/core/table_copy.wast:3409
assert_trap(() => invoke($72, `test`, [61]), `uninitialized element`);

// ./test/core/table_copy.wast:3410
assert_trap(() => invoke($72, `test`, [62]), `uninitialized element`);

// ./test/core/table_copy.wast:3411
assert_trap(() => invoke($72, `test`, [63]), `uninitialized element`);

// ./test/core/table_copy.wast:3412
assert_trap(() => invoke($72, `test`, [64]), `uninitialized element`);

// ./test/core/table_copy.wast:3413
assert_trap(() => invoke($72, `test`, [65]), `uninitialized element`);

// ./test/core/table_copy.wast:3414
assert_trap(() => invoke($72, `test`, [66]), `uninitialized element`);

// ./test/core/table_copy.wast:3415
assert_trap(() => invoke($72, `test`, [67]), `uninitialized element`);

// ./test/core/table_copy.wast:3416
assert_trap(() => invoke($72, `test`, [68]), `uninitialized element`);

// ./test/core/table_copy.wast:3417
assert_trap(() => invoke($72, `test`, [69]), `uninitialized element`);

// ./test/core/table_copy.wast:3418
assert_trap(() => invoke($72, `test`, [70]), `uninitialized element`);

// ./test/core/table_copy.wast:3419
assert_trap(() => invoke($72, `test`, [71]), `uninitialized element`);

// ./test/core/table_copy.wast:3420
assert_trap(() => invoke($72, `test`, [72]), `uninitialized element`);

// ./test/core/table_copy.wast:3421
assert_trap(() => invoke($72, `test`, [73]), `uninitialized element`);

// ./test/core/table_copy.wast:3422
assert_trap(() => invoke($72, `test`, [74]), `uninitialized element`);

// ./test/core/table_copy.wast:3423
assert_trap(() => invoke($72, `test`, [75]), `uninitialized element`);

// ./test/core/table_copy.wast:3424
assert_trap(() => invoke($72, `test`, [76]), `uninitialized element`);

// ./test/core/table_copy.wast:3425
assert_trap(() => invoke($72, `test`, [77]), `uninitialized element`);

// ./test/core/table_copy.wast:3426
assert_trap(() => invoke($72, `test`, [78]), `uninitialized element`);

// ./test/core/table_copy.wast:3427
assert_trap(() => invoke($72, `test`, [79]), `uninitialized element`);

// ./test/core/table_copy.wast:3428
assert_trap(() => invoke($72, `test`, [80]), `uninitialized element`);

// ./test/core/table_copy.wast:3429
assert_trap(() => invoke($72, `test`, [81]), `uninitialized element`);

// ./test/core/table_copy.wast:3430
assert_trap(() => invoke($72, `test`, [82]), `uninitialized element`);

// ./test/core/table_copy.wast:3431
assert_trap(() => invoke($72, `test`, [83]), `uninitialized element`);

// ./test/core/table_copy.wast:3432
assert_trap(() => invoke($72, `test`, [84]), `uninitialized element`);

// ./test/core/table_copy.wast:3433
assert_trap(() => invoke($72, `test`, [85]), `uninitialized element`);

// ./test/core/table_copy.wast:3434
assert_trap(() => invoke($72, `test`, [86]), `uninitialized element`);

// ./test/core/table_copy.wast:3435
assert_trap(() => invoke($72, `test`, [87]), `uninitialized element`);

// ./test/core/table_copy.wast:3436
assert_trap(() => invoke($72, `test`, [88]), `uninitialized element`);

// ./test/core/table_copy.wast:3437
assert_trap(() => invoke($72, `test`, [89]), `uninitialized element`);

// ./test/core/table_copy.wast:3438
assert_trap(() => invoke($72, `test`, [90]), `uninitialized element`);

// ./test/core/table_copy.wast:3439
assert_trap(() => invoke($72, `test`, [91]), `uninitialized element`);

// ./test/core/table_copy.wast:3440
assert_trap(() => invoke($72, `test`, [92]), `uninitialized element`);

// ./test/core/table_copy.wast:3441
assert_trap(() => invoke($72, `test`, [93]), `uninitialized element`);

// ./test/core/table_copy.wast:3442
assert_trap(() => invoke($72, `test`, [94]), `uninitialized element`);

// ./test/core/table_copy.wast:3443
assert_trap(() => invoke($72, `test`, [95]), `uninitialized element`);

// ./test/core/table_copy.wast:3444
assert_trap(() => invoke($72, `test`, [96]), `uninitialized element`);

// ./test/core/table_copy.wast:3445
assert_trap(() => invoke($72, `test`, [97]), `uninitialized element`);

// ./test/core/table_copy.wast:3446
assert_trap(() => invoke($72, `test`, [98]), `uninitialized element`);

// ./test/core/table_copy.wast:3447
assert_trap(() => invoke($72, `test`, [99]), `uninitialized element`);

// ./test/core/table_copy.wast:3448
assert_trap(() => invoke($72, `test`, [100]), `uninitialized element`);

// ./test/core/table_copy.wast:3449
assert_trap(() => invoke($72, `test`, [101]), `uninitialized element`);

// ./test/core/table_copy.wast:3450
assert_trap(() => invoke($72, `test`, [102]), `uninitialized element`);

// ./test/core/table_copy.wast:3451
assert_trap(() => invoke($72, `test`, [103]), `uninitialized element`);

// ./test/core/table_copy.wast:3452
assert_trap(() => invoke($72, `test`, [104]), `uninitialized element`);

// ./test/core/table_copy.wast:3453
assert_trap(() => invoke($72, `test`, [105]), `uninitialized element`);

// ./test/core/table_copy.wast:3454
assert_trap(() => invoke($72, `test`, [106]), `uninitialized element`);

// ./test/core/table_copy.wast:3455
assert_trap(() => invoke($72, `test`, [107]), `uninitialized element`);

// ./test/core/table_copy.wast:3456
assert_trap(() => invoke($72, `test`, [108]), `uninitialized element`);

// ./test/core/table_copy.wast:3457
assert_trap(() => invoke($72, `test`, [109]), `uninitialized element`);

// ./test/core/table_copy.wast:3458
assert_trap(() => invoke($72, `test`, [110]), `uninitialized element`);

// ./test/core/table_copy.wast:3459
assert_trap(() => invoke($72, `test`, [111]), `uninitialized element`);

// ./test/core/table_copy.wast:3460
assert_return(() => invoke($72, `test`, [112]), [value("i32", 0)]);

// ./test/core/table_copy.wast:3461
assert_return(() => invoke($72, `test`, [113]), [value("i32", 1)]);

// ./test/core/table_copy.wast:3462
assert_return(() => invoke($72, `test`, [114]), [value("i32", 2)]);

// ./test/core/table_copy.wast:3463
assert_return(() => invoke($72, `test`, [115]), [value("i32", 3)]);

// ./test/core/table_copy.wast:3464
assert_return(() => invoke($72, `test`, [116]), [value("i32", 4)]);

// ./test/core/table_copy.wast:3465
assert_return(() => invoke($72, `test`, [117]), [value("i32", 5)]);

// ./test/core/table_copy.wast:3466
assert_return(() => invoke($72, `test`, [118]), [value("i32", 6)]);

// ./test/core/table_copy.wast:3467
assert_return(() => invoke($72, `test`, [119]), [value("i32", 7)]);

// ./test/core/table_copy.wast:3468
assert_return(() => invoke($72, `test`, [120]), [value("i32", 8)]);

// ./test/core/table_copy.wast:3469
assert_return(() => invoke($72, `test`, [121]), [value("i32", 9)]);

// ./test/core/table_copy.wast:3470
assert_return(() => invoke($72, `test`, [122]), [value("i32", 10)]);

// ./test/core/table_copy.wast:3471
assert_return(() => invoke($72, `test`, [123]), [value("i32", 11)]);

// ./test/core/table_copy.wast:3472
assert_return(() => invoke($72, `test`, [124]), [value("i32", 12)]);

// ./test/core/table_copy.wast:3473
assert_return(() => invoke($72, `test`, [125]), [value("i32", 13)]);

// ./test/core/table_copy.wast:3474
assert_return(() => invoke($72, `test`, [126]), [value("i32", 14)]);

// ./test/core/table_copy.wast:3475
assert_return(() => invoke($72, `test`, [127]), [value("i32", 15)]);

// ./test/core/table_copy.wast:3477
let $73 = instantiate(`(module
  (type (func (result i32)))
  (table 128 128 funcref)
  (elem (i32.const 0)
         $$f0 $$f1 $$f2 $$f3 $$f4 $$f5 $$f6 $$f7 $$f8 $$f9 $$f10 $$f11 $$f12 $$f13 $$f14 $$f15)
  (func $$f0 (export "f0") (result i32) (i32.const 0))
  (func $$f1 (export "f1") (result i32) (i32.const 1))
  (func $$f2 (export "f2") (result i32) (i32.const 2))
  (func $$f3 (export "f3") (result i32) (i32.const 3))
  (func $$f4 (export "f4") (result i32) (i32.const 4))
  (func $$f5 (export "f5") (result i32) (i32.const 5))
  (func $$f6 (export "f6") (result i32) (i32.const 6))
  (func $$f7 (export "f7") (result i32) (i32.const 7))
  (func $$f8 (export "f8") (result i32) (i32.const 8))
  (func $$f9 (export "f9") (result i32) (i32.const 9))
  (func $$f10 (export "f10") (result i32) (i32.const 10))
  (func $$f11 (export "f11") (result i32) (i32.const 11))
  (func $$f12 (export "f12") (result i32) (i32.const 12))
  (func $$f13 (export "f13") (result i32) (i32.const 13))
  (func $$f14 (export "f14") (result i32) (i32.const 14))
  (func $$f15 (export "f15") (result i32) (i32.const 15))
  (func (export "test") (param $$n i32) (result i32)
    (call_indirect (type 0) (local.get $$n)))
  (func (export "run") (param $$targetOffs i32) (param $$srcOffs i32) (param $$len i32)
    (table.copy (local.get $$targetOffs) (local.get $$srcOffs) (local.get $$len))))`);

// ./test/core/table_copy.wast:3503
assert_trap(() => invoke($73, `run`, [112, 0, -32]), `out of bounds table access`);

// ./test/core/table_copy.wast:3505
assert_return(() => invoke($73, `test`, [0]), [value("i32", 0)]);

// ./test/core/table_copy.wast:3506
assert_return(() => invoke($73, `test`, [1]), [value("i32", 1)]);

// ./test/core/table_copy.wast:3507
assert_return(() => invoke($73, `test`, [2]), [value("i32", 2)]);

// ./test/core/table_copy.wast:3508
assert_return(() => invoke($73, `test`, [3]), [value("i32", 3)]);

// ./test/core/table_copy.wast:3509
assert_return(() => invoke($73, `test`, [4]), [value("i32", 4)]);

// ./test/core/table_copy.wast:3510
assert_return(() => invoke($73, `test`, [5]), [value("i32", 5)]);

// ./test/core/table_copy.wast:3511
assert_return(() => invoke($73, `test`, [6]), [value("i32", 6)]);

// ./test/core/table_copy.wast:3512
assert_return(() => invoke($73, `test`, [7]), [value("i32", 7)]);

// ./test/core/table_copy.wast:3513
assert_return(() => invoke($73, `test`, [8]), [value("i32", 8)]);

// ./test/core/table_copy.wast:3514
assert_return(() => invoke($73, `test`, [9]), [value("i32", 9)]);

// ./test/core/table_copy.wast:3515
assert_return(() => invoke($73, `test`, [10]), [value("i32", 10)]);

// ./test/core/table_copy.wast:3516
assert_return(() => invoke($73, `test`, [11]), [value("i32", 11)]);

// ./test/core/table_copy.wast:3517
assert_return(() => invoke($73, `test`, [12]), [value("i32", 12)]);

// ./test/core/table_copy.wast:3518
assert_return(() => invoke($73, `test`, [13]), [value("i32", 13)]);

// ./test/core/table_copy.wast:3519
assert_return(() => invoke($73, `test`, [14]), [value("i32", 14)]);

// ./test/core/table_copy.wast:3520
assert_return(() => invoke($73, `test`, [15]), [value("i32", 15)]);

// ./test/core/table_copy.wast:3521
assert_trap(() => invoke($73, `test`, [16]), `uninitialized element`);

// ./test/core/table_copy.wast:3522
assert_trap(() => invoke($73, `test`, [17]), `uninitialized element`);

// ./test/core/table_copy.wast:3523
assert_trap(() => invoke($73, `test`, [18]), `uninitialized element`);

// ./test/core/table_copy.wast:3524
assert_trap(() => invoke($73, `test`, [19]), `uninitialized element`);

// ./test/core/table_copy.wast:3525
assert_trap(() => invoke($73, `test`, [20]), `uninitialized element`);

// ./test/core/table_copy.wast:3526
assert_trap(() => invoke($73, `test`, [21]), `uninitialized element`);

// ./test/core/table_copy.wast:3527
assert_trap(() => invoke($73, `test`, [22]), `uninitialized element`);

// ./test/core/table_copy.wast:3528
assert_trap(() => invoke($73, `test`, [23]), `uninitialized element`);

// ./test/core/table_copy.wast:3529
assert_trap(() => invoke($73, `test`, [24]), `uninitialized element`);

// ./test/core/table_copy.wast:3530
assert_trap(() => invoke($73, `test`, [25]), `uninitialized element`);

// ./test/core/table_copy.wast:3531
assert_trap(() => invoke($73, `test`, [26]), `uninitialized element`);

// ./test/core/table_copy.wast:3532
assert_trap(() => invoke($73, `test`, [27]), `uninitialized element`);

// ./test/core/table_copy.wast:3533
assert_trap(() => invoke($73, `test`, [28]), `uninitialized element`);

// ./test/core/table_copy.wast:3534
assert_trap(() => invoke($73, `test`, [29]), `uninitialized element`);

// ./test/core/table_copy.wast:3535
assert_trap(() => invoke($73, `test`, [30]), `uninitialized element`);

// ./test/core/table_copy.wast:3536
assert_trap(() => invoke($73, `test`, [31]), `uninitialized element`);

// ./test/core/table_copy.wast:3537
assert_trap(() => invoke($73, `test`, [32]), `uninitialized element`);

// ./test/core/table_copy.wast:3538
assert_trap(() => invoke($73, `test`, [33]), `uninitialized element`);

// ./test/core/table_copy.wast:3539
assert_trap(() => invoke($73, `test`, [34]), `uninitialized element`);

// ./test/core/table_copy.wast:3540
assert_trap(() => invoke($73, `test`, [35]), `uninitialized element`);

// ./test/core/table_copy.wast:3541
assert_trap(() => invoke($73, `test`, [36]), `uninitialized element`);

// ./test/core/table_copy.wast:3542
assert_trap(() => invoke($73, `test`, [37]), `uninitialized element`);

// ./test/core/table_copy.wast:3543
assert_trap(() => invoke($73, `test`, [38]), `uninitialized element`);

// ./test/core/table_copy.wast:3544
assert_trap(() => invoke($73, `test`, [39]), `uninitialized element`);

// ./test/core/table_copy.wast:3545
assert_trap(() => invoke($73, `test`, [40]), `uninitialized element`);

// ./test/core/table_copy.wast:3546
assert_trap(() => invoke($73, `test`, [41]), `uninitialized element`);

// ./test/core/table_copy.wast:3547
assert_trap(() => invoke($73, `test`, [42]), `uninitialized element`);

// ./test/core/table_copy.wast:3548
assert_trap(() => invoke($73, `test`, [43]), `uninitialized element`);

// ./test/core/table_copy.wast:3549
assert_trap(() => invoke($73, `test`, [44]), `uninitialized element`);

// ./test/core/table_copy.wast:3550
assert_trap(() => invoke($73, `test`, [45]), `uninitialized element`);

// ./test/core/table_copy.wast:3551
assert_trap(() => invoke($73, `test`, [46]), `uninitialized element`);

// ./test/core/table_copy.wast:3552
assert_trap(() => invoke($73, `test`, [47]), `uninitialized element`);

// ./test/core/table_copy.wast:3553
assert_trap(() => invoke($73, `test`, [48]), `uninitialized element`);

// ./test/core/table_copy.wast:3554
assert_trap(() => invoke($73, `test`, [49]), `uninitialized element`);

// ./test/core/table_copy.wast:3555
assert_trap(() => invoke($73, `test`, [50]), `uninitialized element`);

// ./test/core/table_copy.wast:3556
assert_trap(() => invoke($73, `test`, [51]), `uninitialized element`);

// ./test/core/table_copy.wast:3557
assert_trap(() => invoke($73, `test`, [52]), `uninitialized element`);

// ./test/core/table_copy.wast:3558
assert_trap(() => invoke($73, `test`, [53]), `uninitialized element`);

// ./test/core/table_copy.wast:3559
assert_trap(() => invoke($73, `test`, [54]), `uninitialized element`);

// ./test/core/table_copy.wast:3560
assert_trap(() => invoke($73, `test`, [55]), `uninitialized element`);

// ./test/core/table_copy.wast:3561
assert_trap(() => invoke($73, `test`, [56]), `uninitialized element`);

// ./test/core/table_copy.wast:3562
assert_trap(() => invoke($73, `test`, [57]), `uninitialized element`);

// ./test/core/table_copy.wast:3563
assert_trap(() => invoke($73, `test`, [58]), `uninitialized element`);

// ./test/core/table_copy.wast:3564
assert_trap(() => invoke($73, `test`, [59]), `uninitialized element`);

// ./test/core/table_copy.wast:3565
assert_trap(() => invoke($73, `test`, [60]), `uninitialized element`);

// ./test/core/table_copy.wast:3566
assert_trap(() => invoke($73, `test`, [61]), `uninitialized element`);

// ./test/core/table_copy.wast:3567
assert_trap(() => invoke($73, `test`, [62]), `uninitialized element`);

// ./test/core/table_copy.wast:3568
assert_trap(() => invoke($73, `test`, [63]), `uninitialized element`);

// ./test/core/table_copy.wast:3569
assert_trap(() => invoke($73, `test`, [64]), `uninitialized element`);

// ./test/core/table_copy.wast:3570
assert_trap(() => invoke($73, `test`, [65]), `uninitialized element`);

// ./test/core/table_copy.wast:3571
assert_trap(() => invoke($73, `test`, [66]), `uninitialized element`);

// ./test/core/table_copy.wast:3572
assert_trap(() => invoke($73, `test`, [67]), `uninitialized element`);

// ./test/core/table_copy.wast:3573
assert_trap(() => invoke($73, `test`, [68]), `uninitialized element`);

// ./test/core/table_copy.wast:3574
assert_trap(() => invoke($73, `test`, [69]), `uninitialized element`);

// ./test/core/table_copy.wast:3575
assert_trap(() => invoke($73, `test`, [70]), `uninitialized element`);

// ./test/core/table_copy.wast:3576
assert_trap(() => invoke($73, `test`, [71]), `uninitialized element`);

// ./test/core/table_copy.wast:3577
assert_trap(() => invoke($73, `test`, [72]), `uninitialized element`);

// ./test/core/table_copy.wast:3578
assert_trap(() => invoke($73, `test`, [73]), `uninitialized element`);

// ./test/core/table_copy.wast:3579
assert_trap(() => invoke($73, `test`, [74]), `uninitialized element`);

// ./test/core/table_copy.wast:3580
assert_trap(() => invoke($73, `test`, [75]), `uninitialized element`);

// ./test/core/table_copy.wast:3581
assert_trap(() => invoke($73, `test`, [76]), `uninitialized element`);

// ./test/core/table_copy.wast:3582
assert_trap(() => invoke($73, `test`, [77]), `uninitialized element`);

// ./test/core/table_copy.wast:3583
assert_trap(() => invoke($73, `test`, [78]), `uninitialized element`);

// ./test/core/table_copy.wast:3584
assert_trap(() => invoke($73, `test`, [79]), `uninitialized element`);

// ./test/core/table_copy.wast:3585
assert_trap(() => invoke($73, `test`, [80]), `uninitialized element`);

// ./test/core/table_copy.wast:3586
assert_trap(() => invoke($73, `test`, [81]), `uninitialized element`);

// ./test/core/table_copy.wast:3587
assert_trap(() => invoke($73, `test`, [82]), `uninitialized element`);

// ./test/core/table_copy.wast:3588
assert_trap(() => invoke($73, `test`, [83]), `uninitialized element`);

// ./test/core/table_copy.wast:3589
assert_trap(() => invoke($73, `test`, [84]), `uninitialized element`);

// ./test/core/table_copy.wast:3590
assert_trap(() => invoke($73, `test`, [85]), `uninitialized element`);

// ./test/core/table_copy.wast:3591
assert_trap(() => invoke($73, `test`, [86]), `uninitialized element`);

// ./test/core/table_copy.wast:3592
assert_trap(() => invoke($73, `test`, [87]), `uninitialized element`);

// ./test/core/table_copy.wast:3593
assert_trap(() => invoke($73, `test`, [88]), `uninitialized element`);

// ./test/core/table_copy.wast:3594
assert_trap(() => invoke($73, `test`, [89]), `uninitialized element`);

// ./test/core/table_copy.wast:3595
assert_trap(() => invoke($73, `test`, [90]), `uninitialized element`);

// ./test/core/table_copy.wast:3596
assert_trap(() => invoke($73, `test`, [91]), `uninitialized element`);

// ./test/core/table_copy.wast:3597
assert_trap(() => invoke($73, `test`, [92]), `uninitialized element`);

// ./test/core/table_copy.wast:3598
assert_trap(() => invoke($73, `test`, [93]), `uninitialized element`);

// ./test/core/table_copy.wast:3599
assert_trap(() => invoke($73, `test`, [94]), `uninitialized element`);

// ./test/core/table_copy.wast:3600
assert_trap(() => invoke($73, `test`, [95]), `uninitialized element`);

// ./test/core/table_copy.wast:3601
assert_trap(() => invoke($73, `test`, [96]), `uninitialized element`);

// ./test/core/table_copy.wast:3602
assert_trap(() => invoke($73, `test`, [97]), `uninitialized element`);

// ./test/core/table_copy.wast:3603
assert_trap(() => invoke($73, `test`, [98]), `uninitialized element`);

// ./test/core/table_copy.wast:3604
assert_trap(() => invoke($73, `test`, [99]), `uninitialized element`);

// ./test/core/table_copy.wast:3605
assert_trap(() => invoke($73, `test`, [100]), `uninitialized element`);

// ./test/core/table_copy.wast:3606
assert_trap(() => invoke($73, `test`, [101]), `uninitialized element`);

// ./test/core/table_copy.wast:3607
assert_trap(() => invoke($73, `test`, [102]), `uninitialized element`);

// ./test/core/table_copy.wast:3608
assert_trap(() => invoke($73, `test`, [103]), `uninitialized element`);

// ./test/core/table_copy.wast:3609
assert_trap(() => invoke($73, `test`, [104]), `uninitialized element`);

// ./test/core/table_copy.wast:3610
assert_trap(() => invoke($73, `test`, [105]), `uninitialized element`);

// ./test/core/table_copy.wast:3611
assert_trap(() => invoke($73, `test`, [106]), `uninitialized element`);

// ./test/core/table_copy.wast:3612
assert_trap(() => invoke($73, `test`, [107]), `uninitialized element`);

// ./test/core/table_copy.wast:3613
assert_trap(() => invoke($73, `test`, [108]), `uninitialized element`);

// ./test/core/table_copy.wast:3614
assert_trap(() => invoke($73, `test`, [109]), `uninitialized element`);

// ./test/core/table_copy.wast:3615
assert_trap(() => invoke($73, `test`, [110]), `uninitialized element`);

// ./test/core/table_copy.wast:3616
assert_trap(() => invoke($73, `test`, [111]), `uninitialized element`);

// ./test/core/table_copy.wast:3617
assert_trap(() => invoke($73, `test`, [112]), `uninitialized element`);

// ./test/core/table_copy.wast:3618
assert_trap(() => invoke($73, `test`, [113]), `uninitialized element`);

// ./test/core/table_copy.wast:3619
assert_trap(() => invoke($73, `test`, [114]), `uninitialized element`);

// ./test/core/table_copy.wast:3620
assert_trap(() => invoke($73, `test`, [115]), `uninitialized element`);

// ./test/core/table_copy.wast:3621
assert_trap(() => invoke($73, `test`, [116]), `uninitialized element`);

// ./test/core/table_copy.wast:3622
assert_trap(() => invoke($73, `test`, [117]), `uninitialized element`);

// ./test/core/table_copy.wast:3623
assert_trap(() => invoke($73, `test`, [118]), `uninitialized element`);

// ./test/core/table_copy.wast:3624
assert_trap(() => invoke($73, `test`, [119]), `uninitialized element`);

// ./test/core/table_copy.wast:3625
assert_trap(() => invoke($73, `test`, [120]), `uninitialized element`);

// ./test/core/table_copy.wast:3626
assert_trap(() => invoke($73, `test`, [121]), `uninitialized element`);

// ./test/core/table_copy.wast:3627
assert_trap(() => invoke($73, `test`, [122]), `uninitialized element`);

// ./test/core/table_copy.wast:3628
assert_trap(() => invoke($73, `test`, [123]), `uninitialized element`);

// ./test/core/table_copy.wast:3629
assert_trap(() => invoke($73, `test`, [124]), `uninitialized element`);

// ./test/core/table_copy.wast:3630
assert_trap(() => invoke($73, `test`, [125]), `uninitialized element`);

// ./test/core/table_copy.wast:3631
assert_trap(() => invoke($73, `test`, [126]), `uninitialized element`);

// ./test/core/table_copy.wast:3632
assert_trap(() => invoke($73, `test`, [127]), `uninitialized element`);
