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

// ./test/core/table_copy_mixed.wast

// ./test/core/table_copy_mixed.wast:2
let $0 = instantiate(`(module
  (table $$t32 30 30 funcref)
  (table $$t64 i64 30 30 funcref)

  (func (export "test32")
    (table.copy $$t32 $$t32 (i32.const 13) (i32.const 2) (i32.const 3)))

  (func (export "test64")
    (table.copy $$t64 $$t64 (i64.const 13) (i64.const 2) (i64.const 3)))

  (func (export "test_64to32")
    (table.copy $$t32 $$t64 (i32.const 13) (i64.const 2) (i32.const 3)))

  (func (export "test_32to64")
    (table.copy $$t64 $$t32 (i64.const 13) (i32.const 2) (i32.const 3)))
)`);

// ./test/core/table_copy_mixed.wast:20
assert_invalid(
  () => instantiate(`(module
  (table $$t32 30 30 funcref)
  (table $$t64 i64 30 30 funcref)

  (func (export "bad_size_arg")
    (table.copy $$t32 $$t64 (i32.const 13) (i64.const 2) (i64.const 3)))
  )`),
  `type mismatch`,
);

// ./test/core/table_copy_mixed.wast:30
assert_invalid(
  () => instantiate(`(module
  (table $$t32 30 30 funcref)
  (table $$t64 i64 30 30 funcref)

  (func (export "bad_src_idx")
    (table.copy $$t32 $$t64 (i32.const 13) (i32.const 2) (i32.const 3)))
  )`),
  `type mismatch`,
);

// ./test/core/table_copy_mixed.wast:40
assert_invalid(
  () => instantiate(`(module
  (table $$t32 30 30 funcref)
  (table $$t64 i64 30 30 funcref)

  (func (export "bad_dst_idx")
    (table.copy $$t32 $$t64 (i64.const 13) (i64.const 2) (i32.const 3)))
  )`),
  `type mismatch`,
);
