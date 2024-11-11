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

// ./test/core/local_init.wast

// ./test/core/local_init.wast:3
let $0 = instantiate(`(module
  (func (export "get-after-set") (param $$p (ref extern)) (result (ref extern))
    (local $$x (ref extern))
    (local.set $$x (local.get $$p))
    (local.get $$x)
  )
  (func (export "get-after-tee") (param $$p (ref extern)) (result (ref extern))
    (local $$x (ref extern))
    (drop (local.tee $$x (local.get $$p)))
    (local.get $$x)
  )
  (func (export "get-in-block-after-set") (param $$p (ref extern)) (result (ref extern))
    (local $$x (ref extern))
    (local.set $$x (local.get $$p))
    (block (result (ref extern)) (local.get $$x))
  )
)`);

// ./test/core/local_init.wast:21
assert_return(() => invoke($0, `get-after-set`, [externref(1)]), [new ExternRefResult(1)]);

// ./test/core/local_init.wast:22
assert_return(() => invoke($0, `get-after-tee`, [externref(2)]), [new ExternRefResult(2)]);

// ./test/core/local_init.wast:23
assert_return(() => invoke($0, `get-in-block-after-set`, [externref(3)]), [new ExternRefResult(3)]);

// ./test/core/local_init.wast:25
assert_invalid(
  () => instantiate(`(module (func $$uninit (local $$x (ref extern)) (drop (local.get $$x))))`),
  `uninitialized local`,
);

// ./test/core/local_init.wast:29
assert_invalid(
  () => instantiate(`(module
    (func $$uninit-after-end (param $$p (ref extern))
      (local $$x (ref extern))
      (block (local.set $$x (local.get $$p)) (drop (local.tee $$x (local.get $$p))))
      (drop (local.get $$x))
    )
  )`),
  `uninitialized local`,
);

// ./test/core/local_init.wast:39
assert_invalid(
  () => instantiate(`(module
    (func $$uninit-in-else (param $$p (ref extern))
      (local $$x (ref extern))
      (if (i32.const 0)
        (then (local.set $$x (local.get $$p)))
	(else (local.get $$x))
      )
    )
  )`),
  `uninitialized local`,
);

// ./test/core/local_init.wast:52
assert_invalid(
  () => instantiate(`(module
    (func $$uninit-from-if (param $$p (ref extern))
      (local $$x (ref extern))
      (if (i32.const 0)
        (then (local.set $$x (local.get $$p)))
	(else (local.set $$x (local.get $$p)))
      )
      (drop (local.get $$x))
    )
  )`),
  `uninitialized local`,
);

// ./test/core/local_init.wast:66
let $1 = instantiate(`(module
  (func (export "tee-init") (param $$p (ref extern)) (result (ref extern))
    (local $$x (ref extern))
    (drop (local.tee $$x (local.get $$p)))
    (local.get $$x)
  )
)`);

// ./test/core/local_init.wast:74
assert_return(() => invoke($1, `tee-init`, [externref(1)]), [new ExternRefResult(1)]);
