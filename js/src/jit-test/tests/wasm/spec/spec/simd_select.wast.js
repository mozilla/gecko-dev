// |jit-test| skip-if: !wasmSimdEnabled()

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

// ./test/core/simd/simd_select.wast

// ./test/core/simd/simd_select.wast:3
let $0 = instantiate(`(module
  (func (export "select_v128_i32") (param v128 v128 i32) (result v128)
    (select (local.get 0) (local.get 1) (local.get 2))
  )
)`);

// ./test/core/simd/simd_select.wast:9
assert_return(
  () => invoke($0, `select_v128_i32`, [
    i32x4([0x1, 0x2, 0x3, 0x4]),
    i32x4([0x5, 0x6, 0x7, 0x8]),
    1,
  ]),
  [i32x4([0x1, 0x2, 0x3, 0x4])],
);

// ./test/core/simd/simd_select.wast:18
assert_return(
  () => invoke($0, `select_v128_i32`, [
    i32x4([0x1, 0x2, 0x3, 0x4]),
    i32x4([0x5, 0x6, 0x7, 0x8]),
    0,
  ]),
  [i32x4([0x5, 0x6, 0x7, 0x8])],
);

// ./test/core/simd/simd_select.wast:27
assert_return(
  () => invoke($0, `select_v128_i32`, [f32x4([1, 2, 3, 4]), f32x4([5, 6, 7, 8]), -1]),
  [
    new F32x4Pattern(
      value("f32", 1),
      value("f32", 2),
      value("f32", 3),
      value("f32", 4),
    ),
  ],
);

// ./test/core/simd/simd_select.wast:36
assert_return(
  () => invoke($0, `select_v128_i32`, [
    f32x4([-1.5, -2.5, -3.5, -4.5]),
    f32x4([9.5, 8.5, 7.5, 6.5]),
    0,
  ]),
  [
    new F32x4Pattern(
      value("f32", 9.5),
      value("f32", 8.5),
      value("f32", 7.5),
      value("f32", 6.5),
    ),
  ],
);

// ./test/core/simd/simd_select.wast:45
assert_return(
  () => invoke($0, `select_v128_i32`, [
    i8x16([0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10]),
    i8x16([0x10, 0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1]),
    123,
  ]),
  [
    i8x16([0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10]),
  ],
);

// ./test/core/simd/simd_select.wast:54
assert_return(
  () => invoke($0, `select_v128_i32`, [
    i8x16([0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1]),
    i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
    0,
  ]),
  [
    i8x16([0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0]),
  ],
);
