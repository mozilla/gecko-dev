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

// ./test/core/threads/wait_notify.wast

// ./test/core/threads/wait_notify.wast:2
let $0 = instantiate(`(module \$Mem
  (memory (export "shared") 1 1 shared)
)`);
let $Mem = $0;

// ./test/core/threads/wait_notify.wast:6
let $T1 = new Thread($Mem, "$Mem", `

// ./test/core/threads/wait_notify.wast:7:3
register(\$Mem, \`mem\`);

// ./test/core/threads/wait_notify.wast:8:3
let \$1 = instantiate(\`(module
    (memory (import "mem" "shared") 1 10 shared)
    (func (export "run") (result i32)
      (memory.atomic.wait32 (i32.const 0) (i32.const 0) (i64.const -1))
    )
  )\`);

// ./test/core/threads/wait_notify.wast:15:3
assert_return(() => invoke(\$1, \`run\`, []), [value("i32", 0)]);
`);

// ./test/core/threads/wait_notify.wast:18
let $T2 = new Thread($Mem, "$Mem", `

// ./test/core/threads/wait_notify.wast:19:3
register(\$Mem, \`mem\`);

// ./test/core/threads/wait_notify.wast:20:3
let \$2 = instantiate(\`(module
    (memory (import "mem" "shared") 1 1 shared)
    (func (export "notify-0") (result i32)
      (memory.atomic.notify (i32.const 0) (i32.const 0))
    )
    (func (export "notify-1-while")
      (loop
        (i32.const 1)
        (memory.atomic.notify (i32.const 0) (i32.const 1))
        (i32.ne)
        (br_if 0)
      )
    )
  )\`);

// ./test/core/threads/wait_notify.wast:35:3
assert_return(() => invoke(\$2, \`notify-0\`, []), [value("i32", 0)]);

// ./test/core/threads/wait_notify.wast:37:3
assert_return(() => invoke(\$2, \`notify-1-while\`, []), []);
`);

// ./test/core/threads/wait_notify.wast:40
$T1.wait();

// ./test/core/threads/wait_notify.wast:41
$T2.wait();
