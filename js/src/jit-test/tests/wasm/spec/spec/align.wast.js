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

// ./test/core/align.wast

// ./test/core/align.wast:3
let $0 = instantiate(`(module (memory 0) (func (drop (i32.load8_s align=1 (i32.const 0)))))`);

// ./test/core/align.wast:4
let $1 = instantiate(`(module (memory 0) (func (drop (i32.load8_u align=1 (i32.const 0)))))`);

// ./test/core/align.wast:5
let $2 = instantiate(`(module (memory 0) (func (drop (i32.load16_s align=2 (i32.const 0)))))`);

// ./test/core/align.wast:6
let $3 = instantiate(`(module (memory 0) (func (drop (i32.load16_u align=2 (i32.const 0)))))`);

// ./test/core/align.wast:7
let $4 = instantiate(`(module (memory 0) (func (drop (i32.load align=4 (i32.const 0)))))`);

// ./test/core/align.wast:8
let $5 = instantiate(`(module (memory 0) (func (drop (i64.load8_s align=1 (i32.const 0)))))`);

// ./test/core/align.wast:9
let $6 = instantiate(`(module (memory 0) (func (drop (i64.load8_u align=1 (i32.const 0)))))`);

// ./test/core/align.wast:10
let $7 = instantiate(`(module (memory 0) (func (drop (i64.load16_s align=2 (i32.const 0)))))`);

// ./test/core/align.wast:11
let $8 = instantiate(`(module (memory 0) (func (drop (i64.load16_u align=2 (i32.const 0)))))`);

// ./test/core/align.wast:12
let $9 = instantiate(`(module (memory 0) (func (drop (i64.load32_s align=4 (i32.const 0)))))`);

// ./test/core/align.wast:13
let $10 = instantiate(`(module (memory 0) (func (drop (i64.load32_u align=4 (i32.const 0)))))`);

// ./test/core/align.wast:14
let $11 = instantiate(`(module (memory 0) (func (drop (i64.load align=8 (i32.const 0)))))`);

// ./test/core/align.wast:15
let $12 = instantiate(`(module (memory 0) (func (drop (f32.load align=4 (i32.const 0)))))`);

// ./test/core/align.wast:16
let $13 = instantiate(`(module (memory 0) (func (drop (f64.load align=8 (i32.const 0)))))`);

// ./test/core/align.wast:17
let $14 = instantiate(`(module (memory 0) (func (i32.store8 align=1 (i32.const 0) (i32.const 1))))`);

// ./test/core/align.wast:18
let $15 = instantiate(`(module (memory 0) (func (i32.store16 align=2 (i32.const 0) (i32.const 1))))`);

// ./test/core/align.wast:19
let $16 = instantiate(`(module (memory 0) (func (i32.store align=4 (i32.const 0) (i32.const 1))))`);

// ./test/core/align.wast:20
let $17 = instantiate(`(module (memory 0) (func (i64.store8 align=1 (i32.const 0) (i64.const 1))))`);

// ./test/core/align.wast:21
let $18 = instantiate(`(module (memory 0) (func (i64.store16 align=2 (i32.const 0) (i64.const 1))))`);

// ./test/core/align.wast:22
let $19 = instantiate(`(module (memory 0) (func (i64.store32 align=4 (i32.const 0) (i64.const 1))))`);

// ./test/core/align.wast:23
let $20 = instantiate(`(module (memory 0) (func (i64.store align=8 (i32.const 0) (i64.const 1))))`);

// ./test/core/align.wast:24
let $21 = instantiate(`(module (memory 0) (func (f32.store align=4 (i32.const 0) (f32.const 1.0))))`);

// ./test/core/align.wast:25
let $22 = instantiate(`(module (memory 0) (func (f64.store align=8 (i32.const 0) (f64.const 1.0))))`);

// ./test/core/align.wast:27
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i32.load8_s align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:33
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i32.load8_s align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:39
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i32.load8_u align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:45
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i32.load8_u align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:51
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i32.load16_s align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:57
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i32.load16_s align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:63
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i32.load16_u align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:69
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i32.load16_u align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:75
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i32.load align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:81
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i32.load align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:87
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load8_s align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:93
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load8_s align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:99
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load8_u align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:105
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load8_u align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:111
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load16_s align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:117
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load16_s align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:123
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load16_u align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:129
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load16_u align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:135
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load32_s align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:141
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load32_s align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:147
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load32_u align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:153
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load32_u align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:159
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:165
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (i64.load align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:171
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (f32.load align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:177
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (f32.load align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:183
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (f64.load align=0 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:189
assert_malformed(
  () => instantiate(`(module (memory 0) (func (drop (f64.load align=7 (i32.const 0))))) `),
  `alignment`,
);

// ./test/core/align.wast:196
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i32.store8 align=0 (i32.const 0) (i32.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:202
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i32.store8 align=7 (i32.const 0) (i32.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:208
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i32.store16 align=0 (i32.const 0) (i32.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:214
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i32.store16 align=7 (i32.const 0) (i32.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:220
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i32.store align=0 (i32.const 0) (i32.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:226
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i32.store align=7 (i32.const 0) (i32.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:232
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i64.store8 align=0 (i32.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:238
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i64.store8 align=7 (i32.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:244
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i64.store16 align=0 (i32.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:250
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i64.store16 align=7 (i32.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:256
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i64.store32 align=0 (i32.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:262
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i64.store32 align=7 (i32.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:268
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i64.store align=0 (i32.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:274
assert_malformed(
  () => instantiate(`(module (memory 0) (func (i64.store align=7 (i32.const 0) (i64.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:280
assert_malformed(
  () => instantiate(`(module (memory 0) (func (f32.store align=0 (i32.const 0) (f32.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:286
assert_malformed(
  () => instantiate(`(module (memory 0) (func (f32.store align=7 (i32.const 0) (f32.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:292
assert_malformed(
  () => instantiate(`(module (memory 0) (func (f64.store align=0 (i32.const 0) (f32.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:298
assert_malformed(
  () => instantiate(`(module (memory 0) (func (f64.store align=7 (i32.const 0) (f32.const 0)))) `),
  `alignment`,
);

// ./test/core/align.wast:305
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i32.load8_s align=2 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:309
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i32.load8_u align=2 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:313
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i32.load16_s align=4 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:317
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i32.load16_u align=4 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:321
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i32.load align=8 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:325
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load8_s align=2 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:329
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load8_u align=2 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:333
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load16_s align=4 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:337
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load16_u align=4 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:341
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load32_s align=8 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:345
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load32_u align=8 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:349
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load align=16 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:353
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (f32.load align=8 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:357
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (f64.load align=16 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:362
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i32.load8_s align=2 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:366
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i32.load8_u align=2 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:370
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i32.load16_s align=4 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:374
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i32.load16_u align=4 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:378
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i32.load align=8 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:382
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load8_s align=2 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:386
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load8_u align=2 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:390
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load16_s align=4 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:394
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load16_u align=4 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:398
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load32_s align=8 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:402
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load32_u align=8 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:406
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (i64.load align=16 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:410
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (f32.load align=8 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:414
assert_invalid(
  () => instantiate(`(module (memory 0) (func (drop (f64.load align=16 (i32.const 0)))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:419
assert_invalid(
  () => instantiate(`(module (memory 0) (func (i32.store8 align=2 (i32.const 0) (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:423
assert_invalid(
  () => instantiate(`(module (memory 0) (func (i32.store16 align=4 (i32.const 0) (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:427
assert_invalid(
  () => instantiate(`(module (memory 0) (func (i32.store align=8 (i32.const 0) (i32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:431
assert_invalid(
  () => instantiate(`(module (memory 0) (func (i64.store8 align=2 (i32.const 0) (i64.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:435
assert_invalid(
  () => instantiate(`(module (memory 0) (func (i64.store16 align=4 (i32.const 0) (i64.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:439
assert_invalid(
  () => instantiate(`(module (memory 0) (func (i64.store32 align=8 (i32.const 0) (i64.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:443
assert_invalid(
  () => instantiate(`(module (memory 0) (func (i64.store align=16 (i32.const 0) (i64.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:447
assert_invalid(
  () => instantiate(`(module (memory 0) (func (f32.store align=8 (i32.const 0) (f32.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:451
assert_invalid(
  () => instantiate(`(module (memory 0) (func (f64.store align=16 (i32.const 0) (f64.const 0))))`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:458
let $23 = instantiate(`(module
  (memory 1)

  ;; $$default: natural alignment, $$1: align=1, $$2: align=2, $$4: align=4, $$8: align=8

  (func (export "f32_align_switch") (param i32) (result f32)
    (local f32 f32)
    (local.set 1 (f32.const 10.0))
    (block $$4
      (block $$2
        (block $$1
          (block $$default
            (block $$0
              (br_table $$0 $$default $$1 $$2 $$4 (local.get 0))
            ) ;; 0
            (f32.store (i32.const 0) (local.get 1))
            (local.set 2 (f32.load (i32.const 0)))
            (br $$4)
          ) ;; default
          (f32.store align=1 (i32.const 0) (local.get 1))
          (local.set 2 (f32.load align=1 (i32.const 0)))
          (br $$4)
        ) ;; 1
        (f32.store align=2 (i32.const 0) (local.get 1))
        (local.set 2 (f32.load align=2 (i32.const 0)))
        (br $$4)
      ) ;; 2
      (f32.store align=4 (i32.const 0) (local.get 1))
      (local.set 2 (f32.load align=4 (i32.const 0)))
    ) ;; 4
    (local.get 2)
  )

  (func (export "f64_align_switch") (param i32) (result f64)
    (local f64 f64)
    (local.set 1 (f64.const 10.0))
    (block $$8
      (block $$4
        (block $$2
          (block $$1
            (block $$default
              (block $$0
                (br_table $$0 $$default $$1 $$2 $$4 $$8 (local.get 0))
              ) ;; 0
              (f64.store (i32.const 0) (local.get 1))
              (local.set 2 (f64.load (i32.const 0)))
              (br $$8)
            ) ;; default
            (f64.store align=1 (i32.const 0) (local.get 1))
            (local.set 2 (f64.load align=1 (i32.const 0)))
            (br $$8)
          ) ;; 1
          (f64.store align=2 (i32.const 0) (local.get 1))
          (local.set 2 (f64.load align=2 (i32.const 0)))
          (br $$8)
        ) ;; 2
        (f64.store align=4 (i32.const 0) (local.get 1))
        (local.set 2 (f64.load align=4 (i32.const 0)))
        (br $$8)
      ) ;; 4
      (f64.store align=8 (i32.const 0) (local.get 1))
      (local.set 2 (f64.load align=8 (i32.const 0)))
    ) ;; 8
    (local.get 2)
  )

  ;; $$8s: i32/i64.load8_s, $$8u: i32/i64.load8_u, $$16s: i32/i64.load16_s, $$16u: i32/i64.load16_u, $$32: i32.load
  ;; $$32s: i64.load32_s, $$32u: i64.load32_u, $$64: i64.load

  (func (export "i32_align_switch") (param i32 i32) (result i32)
    (local i32 i32)
    (local.set 2 (i32.const 10))
    (block $$32
      (block $$16u
        (block $$16s
          (block $$8u
            (block $$8s
              (block $$0
                (br_table $$0 $$8s $$8u $$16s $$16u $$32 (local.get 0))
              ) ;; 0
              (if (i32.eq (local.get 1) (i32.const 0))
                (then
                  (i32.store8 (i32.const 0) (local.get 2))
                  (local.set 3 (i32.load8_s (i32.const 0)))
                )
              )
              (if (i32.eq (local.get 1) (i32.const 1))
                (then
                  (i32.store8 align=1 (i32.const 0) (local.get 2))
                  (local.set 3 (i32.load8_s align=1 (i32.const 0)))
                )
              )
              (br $$32)
            ) ;; 8s
            (if (i32.eq (local.get 1) (i32.const 0))
              (then
                (i32.store8 (i32.const 0) (local.get 2))
                (local.set 3 (i32.load8_u (i32.const 0)))
              )
            )
            (if (i32.eq (local.get 1) (i32.const 1))
              (then
                (i32.store8 align=1 (i32.const 0) (local.get 2))
                (local.set 3 (i32.load8_u align=1 (i32.const 0)))
              )
            )
            (br $$32)
          ) ;; 8u
          (if (i32.eq (local.get 1) (i32.const 0))
            (then
              (i32.store16 (i32.const 0) (local.get 2))
              (local.set 3 (i32.load16_s (i32.const 0)))
            )
          )
          (if (i32.eq (local.get 1) (i32.const 1))
            (then
              (i32.store16 align=1 (i32.const 0) (local.get 2))
              (local.set 3 (i32.load16_s align=1 (i32.const 0)))
            )
          )
          (if (i32.eq (local.get 1) (i32.const 2))
            (then
              (i32.store16 align=2 (i32.const 0) (local.get 2))
              (local.set 3 (i32.load16_s align=2 (i32.const 0)))
            )
          )
          (br $$32)
        ) ;; 16s
        (if (i32.eq (local.get 1) (i32.const 0))
          (then
            (i32.store16 (i32.const 0) (local.get 2))
            (local.set 3 (i32.load16_u (i32.const 0)))
          )
        )
        (if (i32.eq (local.get 1) (i32.const 1))
          (then
            (i32.store16 align=1 (i32.const 0) (local.get 2))
            (local.set 3 (i32.load16_u align=1 (i32.const 0)))
          )
        )
        (if (i32.eq (local.get 1) (i32.const 2))
          (then
            (i32.store16 align=2 (i32.const 0) (local.get 2))
            (local.set 3 (i32.load16_u align=2 (i32.const 0)))
          )
        )
        (br $$32)
      ) ;; 16u
      (if (i32.eq (local.get 1) (i32.const 0))
        (then
          (i32.store (i32.const 0) (local.get 2))
          (local.set 3 (i32.load (i32.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 1))
        (then
          (i32.store align=1 (i32.const 0) (local.get 2))
          (local.set 3 (i32.load align=1 (i32.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 2))
        (then
          (i32.store align=2 (i32.const 0) (local.get 2))
          (local.set 3 (i32.load align=2 (i32.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 4))
        (then
          (i32.store align=4 (i32.const 0) (local.get 2))
          (local.set 3 (i32.load align=4 (i32.const 0)))
        )
      )
    ) ;; 32
    (local.get 3)
  )

  (func (export "i64_align_switch") (param i32 i32) (result i64)
    (local i64 i64)
    (local.set 2 (i64.const 10))
    (block $$64
      (block $$32u
        (block $$32s
          (block $$16u
            (block $$16s
              (block $$8u
                (block $$8s
                  (block $$0
                    (br_table $$0 $$8s $$8u $$16s $$16u $$32s $$32u $$64 (local.get 0))
                  ) ;; 0
                  (if (i32.eq (local.get 1) (i32.const 0))
                    (then
                      (i64.store8 (i32.const 0) (local.get 2))
                      (local.set 3 (i64.load8_s (i32.const 0)))
                    )
                  )
                  (if (i32.eq (local.get 1) (i32.const 1))
                    (then
                      (i64.store8 align=1 (i32.const 0) (local.get 2))
                      (local.set 3 (i64.load8_s align=1 (i32.const 0)))
                    )
                  )
                  (br $$64)
                ) ;; 8s
                (if (i32.eq (local.get 1) (i32.const 0))
                  (then
                    (i64.store8 (i32.const 0) (local.get 2))
                    (local.set 3 (i64.load8_u (i32.const 0)))
                  )
                )
                (if (i32.eq (local.get 1) (i32.const 1))
                  (then
                    (i64.store8 align=1 (i32.const 0) (local.get 2))
                    (local.set 3 (i64.load8_u align=1 (i32.const 0)))
                  )
                )
                (br $$64)
              ) ;; 8u
              (if (i32.eq (local.get 1) (i32.const 0))
                (then
                  (i64.store16 (i32.const 0) (local.get 2))
                  (local.set 3 (i64.load16_s (i32.const 0)))
                )
              )
              (if (i32.eq (local.get 1) (i32.const 1))
                (then
                  (i64.store16 align=1 (i32.const 0) (local.get 2))
                  (local.set 3 (i64.load16_s align=1 (i32.const 0)))
                )
              )
              (if (i32.eq (local.get 1) (i32.const 2))
                (then
                  (i64.store16 align=2 (i32.const 0) (local.get 2))
                  (local.set 3 (i64.load16_s align=2 (i32.const 0)))
                )
              )
              (br $$64)
            ) ;; 16s
            (if (i32.eq (local.get 1) (i32.const 0))
              (then
                (i64.store16 (i32.const 0) (local.get 2))
                (local.set 3 (i64.load16_u (i32.const 0)))
              )
            )
            (if (i32.eq (local.get 1) (i32.const 1))
              (then
                (i64.store16 align=1 (i32.const 0) (local.get 2))
                (local.set 3 (i64.load16_u align=1 (i32.const 0)))
              )
            )
            (if (i32.eq (local.get 1) (i32.const 2))
              (then
                (i64.store16 align=2 (i32.const 0) (local.get 2))
                (local.set 3 (i64.load16_u align=2 (i32.const 0)))
              )
            )
            (br $$64)
          ) ;; 16u
          (if (i32.eq (local.get 1) (i32.const 0))
            (then
              (i64.store32 (i32.const 0) (local.get 2))
              (local.set 3 (i64.load32_s (i32.const 0)))
            )
          )
          (if (i32.eq (local.get 1) (i32.const 1))
            (then
              (i64.store32 align=1 (i32.const 0) (local.get 2))
              (local.set 3 (i64.load32_s align=1 (i32.const 0)))
            )
          )
          (if (i32.eq (local.get 1) (i32.const 2))
            (then
              (i64.store32 align=2 (i32.const 0) (local.get 2))
              (local.set 3 (i64.load32_s align=2 (i32.const 0)))
            )
          )
          (if (i32.eq (local.get 1) (i32.const 4))
            (then
              (i64.store32 align=4 (i32.const 0) (local.get 2))
              (local.set 3 (i64.load32_s align=4 (i32.const 0)))
            )
          )
          (br $$64)
        ) ;; 32s
        (if (i32.eq (local.get 1) (i32.const 0))
          (then
            (i64.store32 (i32.const 0) (local.get 2))
            (local.set 3 (i64.load32_u (i32.const 0)))
          )
        )
        (if (i32.eq (local.get 1) (i32.const 1))
          (then
            (i64.store32 align=1 (i32.const 0) (local.get 2))
            (local.set 3 (i64.load32_u align=1 (i32.const 0)))
          )
        )
        (if (i32.eq (local.get 1) (i32.const 2))
          (then
            (i64.store32 align=2 (i32.const 0) (local.get 2))
            (local.set 3 (i64.load32_u align=2 (i32.const 0)))
          )
        )
        (if (i32.eq (local.get 1) (i32.const 4))
          (then
            (i64.store32 align=4 (i32.const 0) (local.get 2))
            (local.set 3 (i64.load32_u align=4 (i32.const 0)))
          )
        )
        (br $$64)
      ) ;; 32u
      (if (i32.eq (local.get 1) (i32.const 0))
        (then
          (i64.store (i32.const 0) (local.get 2))
          (local.set 3 (i64.load (i32.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 1))
        (then
          (i64.store align=1 (i32.const 0) (local.get 2))
          (local.set 3 (i64.load align=1 (i32.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 2))
        (then
          (i64.store align=2 (i32.const 0) (local.get 2))
          (local.set 3 (i64.load align=2 (i32.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 4))
        (then
          (i64.store align=4 (i32.const 0) (local.get 2))
          (local.set 3 (i64.load align=4 (i32.const 0)))
        )
      )
      (if (i32.eq (local.get 1) (i32.const 8))
        (then
          (i64.store align=8 (i32.const 0) (local.get 2))
          (local.set 3 (i64.load align=8 (i32.const 0)))
        )
      )
    ) ;; 64
    (local.get 3)
  )
)`);

// ./test/core/align.wast:802
assert_return(() => invoke($23, `f32_align_switch`, [0]), [value("f32", 10)]);

// ./test/core/align.wast:803
assert_return(() => invoke($23, `f32_align_switch`, [1]), [value("f32", 10)]);

// ./test/core/align.wast:804
assert_return(() => invoke($23, `f32_align_switch`, [2]), [value("f32", 10)]);

// ./test/core/align.wast:805
assert_return(() => invoke($23, `f32_align_switch`, [3]), [value("f32", 10)]);

// ./test/core/align.wast:807
assert_return(() => invoke($23, `f64_align_switch`, [0]), [value("f64", 10)]);

// ./test/core/align.wast:808
assert_return(() => invoke($23, `f64_align_switch`, [1]), [value("f64", 10)]);

// ./test/core/align.wast:809
assert_return(() => invoke($23, `f64_align_switch`, [2]), [value("f64", 10)]);

// ./test/core/align.wast:810
assert_return(() => invoke($23, `f64_align_switch`, [3]), [value("f64", 10)]);

// ./test/core/align.wast:811
assert_return(() => invoke($23, `f64_align_switch`, [4]), [value("f64", 10)]);

// ./test/core/align.wast:813
assert_return(() => invoke($23, `i32_align_switch`, [0, 0]), [value("i32", 10)]);

// ./test/core/align.wast:814
assert_return(() => invoke($23, `i32_align_switch`, [0, 1]), [value("i32", 10)]);

// ./test/core/align.wast:815
assert_return(() => invoke($23, `i32_align_switch`, [1, 0]), [value("i32", 10)]);

// ./test/core/align.wast:816
assert_return(() => invoke($23, `i32_align_switch`, [1, 1]), [value("i32", 10)]);

// ./test/core/align.wast:817
assert_return(() => invoke($23, `i32_align_switch`, [2, 0]), [value("i32", 10)]);

// ./test/core/align.wast:818
assert_return(() => invoke($23, `i32_align_switch`, [2, 1]), [value("i32", 10)]);

// ./test/core/align.wast:819
assert_return(() => invoke($23, `i32_align_switch`, [2, 2]), [value("i32", 10)]);

// ./test/core/align.wast:820
assert_return(() => invoke($23, `i32_align_switch`, [3, 0]), [value("i32", 10)]);

// ./test/core/align.wast:821
assert_return(() => invoke($23, `i32_align_switch`, [3, 1]), [value("i32", 10)]);

// ./test/core/align.wast:822
assert_return(() => invoke($23, `i32_align_switch`, [3, 2]), [value("i32", 10)]);

// ./test/core/align.wast:823
assert_return(() => invoke($23, `i32_align_switch`, [4, 0]), [value("i32", 10)]);

// ./test/core/align.wast:824
assert_return(() => invoke($23, `i32_align_switch`, [4, 1]), [value("i32", 10)]);

// ./test/core/align.wast:825
assert_return(() => invoke($23, `i32_align_switch`, [4, 2]), [value("i32", 10)]);

// ./test/core/align.wast:826
assert_return(() => invoke($23, `i32_align_switch`, [4, 4]), [value("i32", 10)]);

// ./test/core/align.wast:828
assert_return(() => invoke($23, `i64_align_switch`, [0, 0]), [value("i64", 10n)]);

// ./test/core/align.wast:829
assert_return(() => invoke($23, `i64_align_switch`, [0, 1]), [value("i64", 10n)]);

// ./test/core/align.wast:830
assert_return(() => invoke($23, `i64_align_switch`, [1, 0]), [value("i64", 10n)]);

// ./test/core/align.wast:831
assert_return(() => invoke($23, `i64_align_switch`, [1, 1]), [value("i64", 10n)]);

// ./test/core/align.wast:832
assert_return(() => invoke($23, `i64_align_switch`, [2, 0]), [value("i64", 10n)]);

// ./test/core/align.wast:833
assert_return(() => invoke($23, `i64_align_switch`, [2, 1]), [value("i64", 10n)]);

// ./test/core/align.wast:834
assert_return(() => invoke($23, `i64_align_switch`, [2, 2]), [value("i64", 10n)]);

// ./test/core/align.wast:835
assert_return(() => invoke($23, `i64_align_switch`, [3, 0]), [value("i64", 10n)]);

// ./test/core/align.wast:836
assert_return(() => invoke($23, `i64_align_switch`, [3, 1]), [value("i64", 10n)]);

// ./test/core/align.wast:837
assert_return(() => invoke($23, `i64_align_switch`, [3, 2]), [value("i64", 10n)]);

// ./test/core/align.wast:838
assert_return(() => invoke($23, `i64_align_switch`, [4, 0]), [value("i64", 10n)]);

// ./test/core/align.wast:839
assert_return(() => invoke($23, `i64_align_switch`, [4, 1]), [value("i64", 10n)]);

// ./test/core/align.wast:840
assert_return(() => invoke($23, `i64_align_switch`, [4, 2]), [value("i64", 10n)]);

// ./test/core/align.wast:841
assert_return(() => invoke($23, `i64_align_switch`, [4, 4]), [value("i64", 10n)]);

// ./test/core/align.wast:842
assert_return(() => invoke($23, `i64_align_switch`, [5, 0]), [value("i64", 10n)]);

// ./test/core/align.wast:843
assert_return(() => invoke($23, `i64_align_switch`, [5, 1]), [value("i64", 10n)]);

// ./test/core/align.wast:844
assert_return(() => invoke($23, `i64_align_switch`, [5, 2]), [value("i64", 10n)]);

// ./test/core/align.wast:845
assert_return(() => invoke($23, `i64_align_switch`, [5, 4]), [value("i64", 10n)]);

// ./test/core/align.wast:846
assert_return(() => invoke($23, `i64_align_switch`, [6, 0]), [value("i64", 10n)]);

// ./test/core/align.wast:847
assert_return(() => invoke($23, `i64_align_switch`, [6, 1]), [value("i64", 10n)]);

// ./test/core/align.wast:848
assert_return(() => invoke($23, `i64_align_switch`, [6, 2]), [value("i64", 10n)]);

// ./test/core/align.wast:849
assert_return(() => invoke($23, `i64_align_switch`, [6, 4]), [value("i64", 10n)]);

// ./test/core/align.wast:850
assert_return(() => invoke($23, `i64_align_switch`, [6, 8]), [value("i64", 10n)]);

// ./test/core/align.wast:854
let $24 = instantiate(`(module
  (memory 1)
  (func (export "store") (param i32 i64)
    (i64.store align=4 (local.get 0) (local.get 1))
  )
  (func (export "load") (param i32) (result i32)
    (i32.load (local.get 0))
  )
)`);

// ./test/core/align.wast:864
assert_trap(() => invoke($24, `store`, [65532, -1n]), `out of bounds memory access`);

// ./test/core/align.wast:866
assert_return(() => invoke($24, `load`, [65532]), [value("i32", 0)]);

// ./test/core/align.wast:872
assert_invalid(
  () => instantiate(`(module binary 
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\02\\01\\00"             ;; Function section: 1 function
    "\\05\\03\\01\\00\\01"          ;; Memory section: 1 memory
    "\\0a\\0a\\01"                ;; Code section: 1 function

    ;; function 0
    "\\08\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\28\\1f\\00"                ;; i32.load offset=0 align=2**31
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:891
assert_invalid(
  () => instantiate(`(module binary 
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\02\\01\\00"             ;; Function section: 1 function
    "\\05\\03\\01\\00\\01"          ;; Memory section: 1 memory
    "\\0a\\0a\\01"                ;; Code section: 1 function

    ;; function 0
    "\\08\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\28\\20\\00"                ;; i32.load offset=0 align=2**32
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:910
assert_invalid(
  () => instantiate(`(module binary 
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\02\\01\\00"             ;; Function section: 1 function
    "\\05\\03\\01\\00\\01"          ;; Memory section: 1 memory
    "\\0a\\0a\\01"                ;; Code section: 1 function

    ;; function 0
    "\\08\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\28\\21\\00"                ;; i32.load offset=0 align=2**33
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:929
assert_invalid(
  () => instantiate(`(module binary 
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\02\\01\\00"             ;; Function section: 1 function
    "\\05\\03\\01\\00\\01"          ;; Memory section: 1 memory
    "\\0a\\0a\\01"                ;; Code section: 1 function

    ;; function 0
    "\\08\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\28\\3f\\00"                ;; i32.load offset=0 align=2**63
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`),
  `alignment must not be larger than natural`,
);

// ./test/core/align.wast:948
assert_invalid(
  () => instantiate(`(module binary 
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\02\\01\\00"             ;; Function section: 1 function
    "\\05\\03\\01\\00\\01"          ;; Memory section: 1 memory
    "\\0a\\0a\\01"                ;; Code section: 1 function

    ;; function 0
    "\\08\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\28\\41\\00"                ;; i32.load offset=0 align=2**65 (parsed as align=1, memidx present)
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`),
  `type mismatch`,
);

// ./test/core/align.wast:967
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\02\\01\\00"             ;; Function section: 1 function
    "\\05\\03\\01\\00\\01"          ;; Memory section: 1 memory
    "\\0a\\0b\\01"                ;; Code section: 1 function

    ;; function 0
    "\\09\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\28\\80\\01\\00"             ;; i32.load offset=0 align="2**128" (malformed)
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`),
  `malformed memop flags`,
);

// ./test/core/align.wast:986
assert_malformed(
  () => instantiate(`(module binary
    "\\00asm" "\\01\\00\\00\\00"
    "\\01\\04\\01\\60\\00\\00"       ;; Type section: 1 type
    "\\03\\02\\01\\00"             ;; Function section: 1 function
    "\\05\\03\\01\\00\\01"          ;; Memory section: 1 memory
    "\\0a\\0b\\01"                ;; Code section: 1 function

    ;; function 0
    "\\09\\00"
    "\\41\\00"                   ;; i32.const 0
    "\\28\\80\\02\\00"             ;; i32.load offset=0 align="2**256" (malformed)
    "\\1a"                      ;; drop
    "\\0b"                      ;; end
  )`),
  `malformed memop flags`,
);

// Suppressed because wasm-tools cannot parse these offsets.
// // ./test/core/align.wast:1005
// let $25 = instantiate(`(module
//   (memory i64 1)
//   (func
//     i64.const 0
//     i32.load offset=0xFFFF_FFFF_FFFF_FFFF
//     drop
//   )
// )`);
//
// // ./test/core/align.wast:1014
// assert_invalid(
//   () => instantiate(`(module
//     (memory 1)
//     (func
//       i32.const 0
//       i32.load offset=0xFFFF_FFFF_FFFF_FFFF align=0x8000_0000_0000_0000
//       drop
//     )
//   )`),
//   `alignment must not be larger than natural`,
// );
