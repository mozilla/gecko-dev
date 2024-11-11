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

// ./test/core/instance.wast

// ./test/core/instance.wast:3
let $M = module(`(module 
  (global (export "glob") (mut i32) (i32.const 0))
  (table (export "tab") 10 funcref (ref.null func))
  (memory (export "mem") 1)
  (tag (export "tag"))
)`);

// ./test/core/instance.wast:10
let $I1 = instantiateFromModule($M);

// ./test/core/instance.wast:11
let $I2 = instantiateFromModule($M);

// ./test/core/instance.wast:12
register($I1, `I1`);

// ./test/core/instance.wast:13
register($I2, `I2`);

// ./test/core/instance.wast:15
let $0 = instantiate(`(module
  (import "I1" "glob" (global $$glob1 (mut i32)))
  (import "I2" "glob" (global $$glob2 (mut i32)))
  (import "I1" "tab" (table $$tab1 10 funcref))
  (import "I2" "tab" (table $$tab2 10 funcref))
  (import "I1" "mem" (memory $$mem1 1))
  (import "I2" "mem" (memory $$mem2 1))
  (import "I1" "tag" (tag $$tag1))
  (import "I2" "tag" (tag $$tag2))

  (func $$f)
  (elem declare func $$f)

  (func (export "glob") (result i32)
    (global.set $$glob1 (i32.const 1))
    (global.get $$glob2)
  )
  (func (export "tab") (result funcref)
    (table.set $$tab1 (i32.const 0) (ref.func $$f))
    (table.get $$tab2 (i32.const 0))
  )
  (func (export "mem") (result i32)
    (i32.store $$mem1 (i32.const 0) (i32.const 1))
    (i32.load $$mem2 (i32.const 0))
  )
  (func (export "tag") (result i32)
    (block $$on_tag1
      (block $$on_other
        (try_table (catch $$tag1 $$on_tag1) (catch_all $$on_other)
          (throw $$tag2)
        )
        (unreachable)
      )
      (return (i32.const 0))
    )
    (return (i32.const 1))
  )
)`);

// ./test/core/instance.wast:54
assert_return(() => invoke($0, `glob`, []), [value("i32", 0)]);

// ./test/core/instance.wast:55
assert_return(() => invoke($0, `tab`, []), [null]);

// ./test/core/instance.wast:56
assert_return(() => invoke($0, `mem`, []), [value("i32", 0)]);

// ./test/core/instance.wast:57
assert_return(() => invoke($0, `tag`, []), [value("i32", 0)]);

// ./test/core/instance.wast:62
let $1 = instantiate(`(module
  (import "I1" "glob" (global $$glob1 (mut i32)))
  (import "I1" "glob" (global $$glob2 (mut i32)))
  (import "I1" "tab" (table $$tab1 10 funcref))
  (import "I1" "tab" (table $$tab2 10 funcref))
  (import "I1" "mem" (memory $$mem1 1))
  (import "I1" "mem" (memory $$mem2 1))
  (import "I1" "tag" (tag $$tag1))
  (import "I1" "tag" (tag $$tag2))

  (func $$f)
  (elem declare func $$f)

  (func (export "glob") (result i32)
    (global.set $$glob1 (i32.const 1))
    (global.get $$glob2)
  )
  (func (export "tab") (result funcref)
    (table.set $$tab1 (i32.const 0) (ref.func $$f))
    (table.get $$tab2 (i32.const 0))
  )
  (func (export "mem") (result i32)
    (i32.store $$mem1 (i32.const 0) (i32.const 1))
    (i32.load $$mem2 (i32.const 0))
  )
  (func (export "tag") (result i32)
    (block $$on_tag1
      (block $$on_other
        (try_table (catch $$tag1 $$on_tag1) (catch_all $$on_other)
          (throw $$tag2)
        )
        (unreachable)
      )
      (return (i32.const 0))
    )
    (return (i32.const 1))
  )
)`);

// ./test/core/instance.wast:101
assert_return(() => invoke($1, `glob`, []), [value("i32", 1)]);

// ./test/core/instance.wast:102
assert_return(() => invoke($1, `tab`, []), [new RefWithType('funcref')]);

// ./test/core/instance.wast:103
assert_return(() => invoke($1, `mem`, []), [value("i32", 1)]);

// ./test/core/instance.wast:104
assert_return(() => invoke($1, `tag`, []), [value("i32", 1)]);

// ./test/core/instance.wast:109
let $N = module(`(module 
  (global $$glob (mut i32) (i32.const 0))
  (table $$tab 10 funcref (ref.null func))
  (memory $$mem 1)
  (tag $$tag)

  (export "glob1" (global $$glob))
  (export "glob2" (global $$glob))
  (export "tab1" (table $$tab))
  (export "tab2" (table $$tab))
  (export "mem1" (memory $$mem))
  (export "mem2" (memory $$mem))
  (export "tag1" (tag $$tag))
  (export "tag2" (tag $$tag))
)`);

// ./test/core/instance.wast:125
let $I = instantiateFromModule($N);

// ./test/core/instance.wast:126
register($I, `I`);

// ./test/core/instance.wast:128
let $2 = instantiate(`(module
  (import "I" "glob1" (global $$glob1 (mut i32)))
  (import "I" "glob2" (global $$glob2 (mut i32)))
  (import "I" "tab1" (table $$tab1 10 funcref))
  (import "I" "tab2" (table $$tab2 10 funcref))
  (import "I" "mem1" (memory $$mem1 1))
  (import "I" "mem2" (memory $$mem2 1))
  (import "I" "tag1" (tag $$tag1))
  (import "I" "tag2" (tag $$tag2))

  (func $$f)
  (elem declare func $$f)

  (func (export "glob") (result i32)
    (global.set $$glob1 (i32.const 1))
    (global.get $$glob2)
  )
  (func (export "tab") (result funcref)
    (table.set $$tab1 (i32.const 0) (ref.func $$f))
    (table.get $$tab2 (i32.const 0))
  )
  (func (export "mem") (result i32)
    (i32.store $$mem1 (i32.const 0) (i32.const 1))
    (i32.load $$mem2 (i32.const 0))
  )
  (func (export "tag") (result i32)
    (block $$on_tag1
      (block $$on_other
        (try_table (catch $$tag1 $$on_tag1) (catch_all $$on_other)
          (throw $$tag2)
        )
        (unreachable)
      )
      (return (i32.const 0))
    )
    (return (i32.const 1))
  )
)`);

// ./test/core/instance.wast:167
assert_return(() => invoke($2, `glob`, []), [value("i32", 1)]);

// ./test/core/instance.wast:168
assert_return(() => invoke($2, `tab`, []), [new RefWithType('funcref')]);

// ./test/core/instance.wast:169
assert_return(() => invoke($2, `mem`, []), [value("i32", 1)]);

// ./test/core/instance.wast:170
assert_return(() => invoke($2, `tag`, []), [value("i32", 1)]);
