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

// ./test/core/threads/LB_atomic.wast

// ./test/core/threads/LB_atomic.wast:1
let $0 = instantiate(`(module \$Mem
  (memory (export "shared") 1 1 shared)
)`);
let $Mem = $0;

// Missing in source test:
// https://github.com/WebAssembly/threads/pull/217
register($Mem, `mem`);

// ./test/core/threads/LB_atomic.wast:5
let $T1 = new Thread($Mem, "$Mem", `

// ./test/core/threads/LB_atomic.wast:6:3
register(\$Mem, \`mem\`);

// ./test/core/threads/LB_atomic.wast:7:3
let \$1 = instantiate(\`(module
    (memory (import "mem" "shared") 1 10 shared)
    (func (export "run")
      (local i32)
      (i32.atomic.load (i32.const 4))
      (local.set 0)
      (i32.atomic.store (i32.const 0) (i32.const 1))

      ;; store results for checking
      (i32.store (i32.const 24) (local.get 0))
    )
  )\`);

// ./test/core/threads/LB_atomic.wast:19:3
invoke(\$1, \`run\`, []);
`);

// ./test/core/threads/LB_atomic.wast:22
let $T2 = new Thread($Mem, "$Mem", `

// ./test/core/threads/LB_atomic.wast:23:3
register(\$Mem, \`mem\`);

// ./test/core/threads/LB_atomic.wast:24:3
let \$2 = instantiate(\`(module
    (memory (import "mem" "shared") 1 1 shared)
    (func (export "run")
      (local i32)
      (i32.atomic.load (i32.const 0))
      (local.set 0)
      (i32.atomic.store (i32.const 4) (i32.const 1))

      ;; store results for checking
      (i32.store (i32.const 32) (local.get 0))
    )
  )\`);

// ./test/core/threads/LB_atomic.wast:37:3
invoke(\$2, \`run\`, []);
`);

// ./test/core/threads/LB_atomic.wast:40
$T1.wait();

// ./test/core/threads/LB_atomic.wast:41
$T2.wait();

// ./test/core/threads/LB_atomic.wast:43
let $3 = instantiate(`(module \$Check
  (memory (import "mem" "shared") 1 1 shared)

  (func (export "check") (result i32)
    (local i32 i32)
    (i32.load (i32.const 24))
    (local.set 0)
    (i32.load (i32.const 32))
    (local.set 1)

    ;; allowed results: (L_0 = 0 && L_1 = 0) || (L_0 = 0 && L_1 = 1) || (L_0 = 1 && L_1 = 0)

    (i32.and (i32.eq (local.get 0) (i32.const 0)) (i32.eq (local.get 1) (i32.const 0)))
    (i32.and (i32.eq (local.get 0) (i32.const 0)) (i32.eq (local.get 1) (i32.const 1)))
    (i32.and (i32.eq (local.get 0) (i32.const 1)) (i32.eq (local.get 1) (i32.const 0)))
    (i32.or)
    (i32.or)
    (return)
  )
)`);
let $Check = $3;

// ./test/core/threads/LB_atomic.wast:64
assert_return(() => invoke($Check, `check`, []), [value("i32", 1)]);
