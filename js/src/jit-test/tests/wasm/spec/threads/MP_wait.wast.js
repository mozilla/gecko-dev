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

// ./test/core/threads/MP_wait.wast

// ./test/core/threads/MP_wait.wast:1
let $0 = instantiate(`(module \$Mem
  (memory (export "shared") 1 1 shared)
)`);
let $Mem = $0;

// Missing in source test:
// https://github.com/WebAssembly/threads/pull/217
register($Mem, `mem`);

// ./test/core/threads/MP_wait.wast:5
let $T1 = new Thread($Mem, "$Mem", `

// ./test/core/threads/MP_wait.wast:6:3
register(\$Mem, \`mem\`);

// ./test/core/threads/MP_wait.wast:7:3
let \$1 = instantiate(\`(module
    (memory (import "mem" "shared") 1 10 shared)
    (func (export "run")
      (i32.atomic.store (i32.const 0) (i32.const 42))
      (i32.atomic.store (i32.const 4) (i32.const 1))
    )
  )\`);

// ./test/core/threads/MP_wait.wast:14:3
invoke(\$1, \`run\`, []);
`);

// ./test/core/threads/MP_wait.wast:17
let $T2 = new Thread($Mem, "$Mem", `

// ./test/core/threads/MP_wait.wast:18:3
register(\$Mem, \`mem\`);

// ./test/core/threads/MP_wait.wast:19:3
let \$2 = instantiate(\`(module
    (memory (import "mem" "shared") 1 1 shared)
    (func (export "run")
      (local i32 i32)
      (i32.atomic.load (i32.const 4))
      (local.set 0)
      (i32.atomic.load (i32.const 0))
      (local.set 1)

      ;; store results for checking
      (i32.store (i32.const 24) (local.get 0))
      (i32.store (i32.const 32) (local.get 1))
    )
  )\`);

// ./test/core/threads/MP_wait.wast:34:3
invoke(\$2, \`run\`, []);
`);

// ./test/core/threads/MP_wait.wast:37
$T1.wait();

// ./test/core/threads/MP_wait.wast:38
$T2.wait();

// ./test/core/threads/MP_wait.wast:40
let $3 = instantiate(`(module \$Check
  (memory (import "mem" "shared") 1 1 shared)

  (func (export "check") (result i32)
    (local i32 i32)
    (i32.load (i32.const 24))
    (local.set 0)
    (i32.load (i32.const 32))
    (local.set 1)

    ;; allowed results: (L_0 = 1 && L_1 = 42) || (L_0 = 0 && L_1 = 0) || (L_0 = 0 && L_1 = 42)

    (i32.and (i32.eq (local.get 0) (i32.const 1)) (i32.eq (local.get 1) (i32.const 42)))
    (i32.and (i32.eq (local.get 0) (i32.const 0)) (i32.eq (local.get 1) (i32.const 0)))
    (i32.and (i32.eq (local.get 0) (i32.const 0)) (i32.eq (local.get 1) (i32.const 42)))
    (i32.or)
    (i32.or)
    (return)
  )
)`);
let $Check = $3;

// ./test/core/threads/MP_wait.wast:61
assert_return(() => invoke($Check, `check`, []), [value("i32", 1)]);
