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

// ./test/core/memory_trap.wast

// ./test/core/memory_trap.wast:1
let $0 = instantiate(`(module
    (memory 1)

    (func $$addr_limit (result i32)
      (i32.mul (memory.size) (i32.const 0x10000))
    )

    (func (export "store") (param $$i i32) (param $$v i32)
      (i32.store (i32.add (call $$addr_limit) (local.get $$i)) (local.get $$v))
    )

    (func (export "load") (param $$i i32) (result i32)
      (i32.load (i32.add (call $$addr_limit) (local.get $$i)))
    )

    (func (export "memory.grow") (param i32) (result i32)
      (memory.grow (local.get 0))
    )
)`);

// ./test/core/memory_trap.wast:21
assert_return(() => invoke($0, `store`, [-4, 42]), []);

// ./test/core/memory_trap.wast:22
assert_return(() => invoke($0, `load`, [-4]), [value("i32", 42)]);

// ./test/core/memory_trap.wast:23
assert_trap(() => invoke($0, `store`, [-3, 305419896]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:24
assert_trap(() => invoke($0, `load`, [-3]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:25
assert_trap(() => invoke($0, `store`, [-2, 13]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:26
assert_trap(() => invoke($0, `load`, [-2]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:27
assert_trap(() => invoke($0, `store`, [-1, 13]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:28
assert_trap(() => invoke($0, `load`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:29
assert_trap(() => invoke($0, `store`, [0, 13]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:30
assert_trap(() => invoke($0, `load`, [0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:31
assert_trap(() => invoke($0, `store`, [-2147483648, 13]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:32
assert_trap(() => invoke($0, `load`, [-2147483648]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:33
assert_return(() => invoke($0, `memory.grow`, [65537]), [value("i32", -1)]);

// ./test/core/memory_trap.wast:35
let $1 = instantiate(`(module
  (memory 1)
  (data (i32.const 0) "abcdefgh")
  (data (i32.const 0xfff8) "abcdefgh")

  (func (export "i32.load") (param $$a i32) (result i32)
    (i32.load (local.get $$a))
  )
  (func (export "i64.load") (param $$a i32) (result i64)
    (i64.load (local.get $$a))
  )
  (func (export "f32.load") (param $$a i32) (result f32)
    (f32.load (local.get $$a))
  )
  (func (export "f64.load") (param $$a i32) (result f64)
    (f64.load (local.get $$a))
  )
  (func (export "i32.load8_s") (param $$a i32) (result i32)
    (i32.load8_s (local.get $$a))
  )
  (func (export "i32.load8_u") (param $$a i32) (result i32)
    (i32.load8_u (local.get $$a))
  )
  (func (export "i32.load16_s") (param $$a i32) (result i32)
    (i32.load16_s (local.get $$a))
  )
  (func (export "i32.load16_u") (param $$a i32) (result i32)
    (i32.load16_u (local.get $$a))
  )
  (func (export "i64.load8_s") (param $$a i32) (result i64)
    (i64.load8_s (local.get $$a))
  )
  (func (export "i64.load8_u") (param $$a i32) (result i64)
    (i64.load8_u (local.get $$a))
  )
  (func (export "i64.load16_s") (param $$a i32) (result i64)
    (i64.load16_s (local.get $$a))
  )
  (func (export "i64.load16_u") (param $$a i32) (result i64)
    (i64.load16_u (local.get $$a))
  )
  (func (export "i64.load32_s") (param $$a i32) (result i64)
    (i64.load32_s (local.get $$a))
  )
  (func (export "i64.load32_u") (param $$a i32) (result i64)
    (i64.load32_u (local.get $$a))
  )
  (func (export "i32.store") (param $$a i32) (param $$v i32)
    (i32.store (local.get $$a) (local.get $$v))
  )
  (func (export "i64.store") (param $$a i32) (param $$v i64)
    (i64.store (local.get $$a) (local.get $$v))
  )
  (func (export "f32.store") (param $$a i32) (param $$v f32)
    (f32.store (local.get $$a) (local.get $$v))
  )
  (func (export "f64.store") (param $$a i32) (param $$v f64)
    (f64.store (local.get $$a) (local.get $$v))
  )
  (func (export "i32.store8") (param $$a i32) (param $$v i32)
    (i32.store8 (local.get $$a) (local.get $$v))
  )
  (func (export "i32.store16") (param $$a i32) (param $$v i32)
    (i32.store16 (local.get $$a) (local.get $$v))
  )
  (func (export "i64.store8") (param $$a i32) (param $$v i64)
    (i64.store8 (local.get $$a) (local.get $$v))
  )
  (func (export "i64.store16") (param $$a i32) (param $$v i64)
    (i64.store16 (local.get $$a) (local.get $$v))
  )
  (func (export "i64.store32") (param $$a i32) (param $$v i64)
    (i64.store32 (local.get $$a) (local.get $$v))
  )
)`);

// ./test/core/memory_trap.wast:111
assert_trap(() => invoke($1, `i32.store`, [65536, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:112
assert_trap(() => invoke($1, `i32.store`, [65535, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:113
assert_trap(() => invoke($1, `i32.store`, [65534, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:114
assert_trap(() => invoke($1, `i32.store`, [65533, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:115
assert_trap(() => invoke($1, `i32.store`, [-1, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:116
assert_trap(() => invoke($1, `i32.store`, [-2, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:117
assert_trap(() => invoke($1, `i32.store`, [-3, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:118
assert_trap(() => invoke($1, `i32.store`, [-4, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:119
assert_trap(() => invoke($1, `i64.store`, [65536, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:120
assert_trap(() => invoke($1, `i64.store`, [65535, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:121
assert_trap(() => invoke($1, `i64.store`, [65534, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:122
assert_trap(() => invoke($1, `i64.store`, [65533, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:123
assert_trap(() => invoke($1, `i64.store`, [65532, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:124
assert_trap(() => invoke($1, `i64.store`, [65531, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:125
assert_trap(() => invoke($1, `i64.store`, [65530, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:126
assert_trap(() => invoke($1, `i64.store`, [65529, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:127
assert_trap(() => invoke($1, `i64.store`, [-1, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:128
assert_trap(() => invoke($1, `i64.store`, [-2, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:129
assert_trap(() => invoke($1, `i64.store`, [-3, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:130
assert_trap(() => invoke($1, `i64.store`, [-4, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:131
assert_trap(() => invoke($1, `i64.store`, [-5, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:132
assert_trap(() => invoke($1, `i64.store`, [-6, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:133
assert_trap(() => invoke($1, `i64.store`, [-7, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:134
assert_trap(() => invoke($1, `i64.store`, [-8, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:135
assert_trap(() => invoke($1, `f32.store`, [65536, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:136
assert_trap(() => invoke($1, `f32.store`, [65535, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:137
assert_trap(() => invoke($1, `f32.store`, [65534, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:138
assert_trap(() => invoke($1, `f32.store`, [65533, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:139
assert_trap(() => invoke($1, `f32.store`, [-1, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:140
assert_trap(() => invoke($1, `f32.store`, [-2, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:141
assert_trap(() => invoke($1, `f32.store`, [-3, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:142
assert_trap(() => invoke($1, `f32.store`, [-4, value("f32", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:143
assert_trap(() => invoke($1, `f64.store`, [65536, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:144
assert_trap(() => invoke($1, `f64.store`, [65535, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:145
assert_trap(() => invoke($1, `f64.store`, [65534, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:146
assert_trap(() => invoke($1, `f64.store`, [65533, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:147
assert_trap(() => invoke($1, `f64.store`, [65532, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:148
assert_trap(() => invoke($1, `f64.store`, [65531, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:149
assert_trap(() => invoke($1, `f64.store`, [65530, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:150
assert_trap(() => invoke($1, `f64.store`, [65529, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:151
assert_trap(() => invoke($1, `f64.store`, [-1, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:152
assert_trap(() => invoke($1, `f64.store`, [-2, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:153
assert_trap(() => invoke($1, `f64.store`, [-3, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:154
assert_trap(() => invoke($1, `f64.store`, [-4, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:155
assert_trap(() => invoke($1, `f64.store`, [-5, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:156
assert_trap(() => invoke($1, `f64.store`, [-6, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:157
assert_trap(() => invoke($1, `f64.store`, [-7, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:158
assert_trap(() => invoke($1, `f64.store`, [-8, value("f64", 0)]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:159
assert_trap(() => invoke($1, `i32.store8`, [65536, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:160
assert_trap(() => invoke($1, `i32.store8`, [-1, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:161
assert_trap(() => invoke($1, `i32.store16`, [65536, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:162
assert_trap(() => invoke($1, `i32.store16`, [65535, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:163
assert_trap(() => invoke($1, `i32.store16`, [-1, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:164
assert_trap(() => invoke($1, `i32.store16`, [-2, 0]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:165
assert_trap(() => invoke($1, `i64.store8`, [65536, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:166
assert_trap(() => invoke($1, `i64.store8`, [-1, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:167
assert_trap(() => invoke($1, `i64.store16`, [65536, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:168
assert_trap(() => invoke($1, `i64.store16`, [65535, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:169
assert_trap(() => invoke($1, `i64.store16`, [-1, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:170
assert_trap(() => invoke($1, `i64.store16`, [-2, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:171
assert_trap(() => invoke($1, `i64.store32`, [65536, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:172
assert_trap(() => invoke($1, `i64.store32`, [65535, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:173
assert_trap(() => invoke($1, `i64.store32`, [65534, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:174
assert_trap(() => invoke($1, `i64.store32`, [65533, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:175
assert_trap(() => invoke($1, `i64.store32`, [-1, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:176
assert_trap(() => invoke($1, `i64.store32`, [-2, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:177
assert_trap(() => invoke($1, `i64.store32`, [-3, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:178
assert_trap(() => invoke($1, `i64.store32`, [-4, 0n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:179
assert_trap(() => invoke($1, `i32.load`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:180
assert_trap(() => invoke($1, `i32.load`, [65535]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:181
assert_trap(() => invoke($1, `i32.load`, [65534]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:182
assert_trap(() => invoke($1, `i32.load`, [65533]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:183
assert_trap(() => invoke($1, `i32.load`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:184
assert_trap(() => invoke($1, `i32.load`, [-2]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:185
assert_trap(() => invoke($1, `i32.load`, [-3]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:186
assert_trap(() => invoke($1, `i32.load`, [-4]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:187
assert_trap(() => invoke($1, `i64.load`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:188
assert_trap(() => invoke($1, `i64.load`, [65535]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:189
assert_trap(() => invoke($1, `i64.load`, [65534]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:190
assert_trap(() => invoke($1, `i64.load`, [65533]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:191
assert_trap(() => invoke($1, `i64.load`, [65532]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:192
assert_trap(() => invoke($1, `i64.load`, [65531]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:193
assert_trap(() => invoke($1, `i64.load`, [65530]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:194
assert_trap(() => invoke($1, `i64.load`, [65529]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:195
assert_trap(() => invoke($1, `i64.load`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:196
assert_trap(() => invoke($1, `i64.load`, [-2]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:197
assert_trap(() => invoke($1, `i64.load`, [-3]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:198
assert_trap(() => invoke($1, `i64.load`, [-4]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:199
assert_trap(() => invoke($1, `i64.load`, [-5]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:200
assert_trap(() => invoke($1, `i64.load`, [-6]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:201
assert_trap(() => invoke($1, `i64.load`, [-7]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:202
assert_trap(() => invoke($1, `i64.load`, [-8]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:203
assert_trap(() => invoke($1, `f32.load`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:204
assert_trap(() => invoke($1, `f32.load`, [65535]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:205
assert_trap(() => invoke($1, `f32.load`, [65534]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:206
assert_trap(() => invoke($1, `f32.load`, [65533]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:207
assert_trap(() => invoke($1, `f32.load`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:208
assert_trap(() => invoke($1, `f32.load`, [-2]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:209
assert_trap(() => invoke($1, `f32.load`, [-3]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:210
assert_trap(() => invoke($1, `f32.load`, [-4]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:211
assert_trap(() => invoke($1, `f64.load`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:212
assert_trap(() => invoke($1, `f64.load`, [65535]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:213
assert_trap(() => invoke($1, `f64.load`, [65534]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:214
assert_trap(() => invoke($1, `f64.load`, [65533]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:215
assert_trap(() => invoke($1, `f64.load`, [65532]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:216
assert_trap(() => invoke($1, `f64.load`, [65531]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:217
assert_trap(() => invoke($1, `f64.load`, [65530]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:218
assert_trap(() => invoke($1, `f64.load`, [65529]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:219
assert_trap(() => invoke($1, `f64.load`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:220
assert_trap(() => invoke($1, `f64.load`, [-2]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:221
assert_trap(() => invoke($1, `f64.load`, [-3]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:222
assert_trap(() => invoke($1, `f64.load`, [-4]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:223
assert_trap(() => invoke($1, `f64.load`, [-5]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:224
assert_trap(() => invoke($1, `f64.load`, [-6]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:225
assert_trap(() => invoke($1, `f64.load`, [-7]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:226
assert_trap(() => invoke($1, `f64.load`, [-8]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:227
assert_trap(() => invoke($1, `i32.load8_s`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:228
assert_trap(() => invoke($1, `i32.load8_s`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:229
assert_trap(() => invoke($1, `i32.load8_u`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:230
assert_trap(() => invoke($1, `i32.load8_u`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:231
assert_trap(() => invoke($1, `i32.load16_s`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:232
assert_trap(() => invoke($1, `i32.load16_s`, [65535]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:233
assert_trap(() => invoke($1, `i32.load16_s`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:234
assert_trap(() => invoke($1, `i32.load16_s`, [-2]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:235
assert_trap(() => invoke($1, `i32.load16_u`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:236
assert_trap(() => invoke($1, `i32.load16_u`, [65535]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:237
assert_trap(() => invoke($1, `i32.load16_u`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:238
assert_trap(() => invoke($1, `i32.load16_u`, [-2]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:239
assert_trap(() => invoke($1, `i64.load8_s`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:240
assert_trap(() => invoke($1, `i64.load8_s`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:241
assert_trap(() => invoke($1, `i64.load8_u`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:242
assert_trap(() => invoke($1, `i64.load8_u`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:243
assert_trap(() => invoke($1, `i64.load16_s`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:244
assert_trap(() => invoke($1, `i64.load16_s`, [65535]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:245
assert_trap(() => invoke($1, `i64.load16_s`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:246
assert_trap(() => invoke($1, `i64.load16_s`, [-2]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:247
assert_trap(() => invoke($1, `i64.load16_u`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:248
assert_trap(() => invoke($1, `i64.load16_u`, [65535]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:249
assert_trap(() => invoke($1, `i64.load16_u`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:250
assert_trap(() => invoke($1, `i64.load16_u`, [-2]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:251
assert_trap(() => invoke($1, `i64.load32_s`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:252
assert_trap(() => invoke($1, `i64.load32_s`, [65535]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:253
assert_trap(() => invoke($1, `i64.load32_s`, [65534]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:254
assert_trap(() => invoke($1, `i64.load32_s`, [65533]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:255
assert_trap(() => invoke($1, `i64.load32_s`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:256
assert_trap(() => invoke($1, `i64.load32_s`, [-2]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:257
assert_trap(() => invoke($1, `i64.load32_s`, [-3]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:258
assert_trap(() => invoke($1, `i64.load32_s`, [-4]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:259
assert_trap(() => invoke($1, `i64.load32_u`, [65536]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:260
assert_trap(() => invoke($1, `i64.load32_u`, [65535]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:261
assert_trap(() => invoke($1, `i64.load32_u`, [65534]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:262
assert_trap(() => invoke($1, `i64.load32_u`, [65533]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:263
assert_trap(() => invoke($1, `i64.load32_u`, [-1]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:264
assert_trap(() => invoke($1, `i64.load32_u`, [-2]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:265
assert_trap(() => invoke($1, `i64.load32_u`, [-3]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:266
assert_trap(() => invoke($1, `i64.load32_u`, [-4]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:269
assert_return(() => invoke($1, `i64.load`, [65528]), [value("i64", 7523094288207667809n)]);

// ./test/core/memory_trap.wast:270
assert_return(() => invoke($1, `i64.load`, [0]), [value("i64", 7523094288207667809n)]);

// ./test/core/memory_trap.wast:274
assert_return(() => invoke($1, `i64.store`, [65528, 0n]), []);

// ./test/core/memory_trap.wast:275
assert_trap(() => invoke($1, `i32.store`, [65533, 305419896]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:276
assert_return(() => invoke($1, `i32.load`, [65532]), [value("i32", 0)]);

// ./test/core/memory_trap.wast:277
assert_trap(() => invoke($1, `i64.store`, [65529, 1311768467294899695n]), `out of bounds memory access`);

// ./test/core/memory_trap.wast:278
assert_return(() => invoke($1, `i64.load`, [65528]), [value("i64", 0n)]);

// ./test/core/memory_trap.wast:279
assert_trap(
  () => invoke($1, `f32.store`, [65533, value("f32", 305419900)]),
  `out of bounds memory access`,
);

// ./test/core/memory_trap.wast:280
assert_return(() => invoke($1, `f32.load`, [65532]), [value("f32", 0)]);

// ./test/core/memory_trap.wast:281
assert_trap(
  () => invoke($1, `f64.store`, [65529, value("f64", 1311768467294899700)]),
  `out of bounds memory access`,
);

// ./test/core/memory_trap.wast:282
assert_return(() => invoke($1, `f64.load`, [65528]), [value("f64", 0)]);
