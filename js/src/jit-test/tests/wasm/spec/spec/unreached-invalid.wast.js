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

// ./test/core/unreached-invalid.wast

// ./test/core/unreached-invalid.wast:3
assert_invalid(
  () => instantiate(`(module (func $$local-index (unreachable) (drop (local.get 0))))`),
  `unknown local`,
);

// ./test/core/unreached-invalid.wast:7
assert_invalid(
  () => instantiate(`(module (func $$global-index (unreachable) (drop (global.get 0))))`),
  `unknown global`,
);

// ./test/core/unreached-invalid.wast:11
assert_invalid(
  () => instantiate(`(module (func $$func-index (unreachable) (call 1)))`),
  `unknown function`,
);

// ./test/core/unreached-invalid.wast:15
assert_invalid(
  () => instantiate(`(module (func $$label-index (unreachable) (br 1)))`),
  `unknown label`,
);

// ./test/core/unreached-invalid.wast:20
assert_invalid(
  () => instantiate(`(module (func $$type-num-vs-num
    (unreachable) (drop (i64.eqz (i32.const 0))))
  )`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:26
assert_invalid(
  () => instantiate(`(module (func $$type-poly-num-vs-num (result i32)
    (unreachable) (i64.const 0) (i32.const 0) (select)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:32
assert_invalid(
  () => instantiate(`(module (func $$type-poly-transitive-num-vs-num (result i32)
    (unreachable)
    (i64.const 0) (i32.const 0) (select)
    (i32.const 0) (i32.const 0) (select)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:41
assert_invalid(
  () => instantiate(`(module (func $$type-unconsumed-const (unreachable) (i32.const 0)))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:45
assert_invalid(
  () => instantiate(`(module (func $$type-unconsumed-result (unreachable) (i32.eqz)))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:49
assert_invalid(
  () => instantiate(`(module (func $$type-unconsumed-result2
    (unreachable) (i32.const 0) (i32.add)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:55
assert_invalid(
  () => instantiate(`(module (func $$type-unconsumed-poly0 (unreachable) (select)))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:59
assert_invalid(
  () => instantiate(`(module (func $$type-unconsumed-poly1 (unreachable) (i32.const 0) (select)))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:63
assert_invalid(
  () => instantiate(`(module (func $$type-unconsumed-poly2
    (unreachable) (i32.const 0) (i32.const 0) (select)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:70
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-void-after-break
    (block (br 0) (block (drop (i32.eqz (nop)))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:76
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-num-after-break
    (block (br 0) (drop (i32.eqz (f32.const 1))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:82
assert_invalid(
  () => instantiate(`(module (func $$type-binary-num-vs-void-after-break
    (block (br 0) (block (drop (f32.eq (i32.const 1)))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:88
assert_invalid(
  () => instantiate(`(module (func $$type-binary-num-vs-num-after-break
    (block (br 0) (drop (f32.eq (i32.const 1) (f32.const 0))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:94
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-void-after-break
    (block (br 0) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:100
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-num-after-break (result i32)
    (block (result i32) (i32.const 1) (br 0) (f32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:106
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-num-vs-void-after-break
    (block (loop (br 1) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:112
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-num-vs-num-after-break (result i32)
    (loop (result i32) (br 1 (i32.const 1)) (f32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:118
assert_invalid(
  () => instantiate(`(module (func $$type-func-value-num-vs-void-after-break
    (br 0) (i32.const 1)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:124
assert_invalid(
  () => instantiate(`(module (func $$type-func-value-num-vs-num-after-break (result i32)
    (br 0 (i32.const 1)) (f32.const 0)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:131
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-void-after-return
    (return) (block (drop (i32.eqz (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:137
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-num-after-return
    (return) (drop (i32.eqz (f32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:143
assert_invalid(
  () => instantiate(`(module (func $$type-binary-num-vs-void-after-return
    (return) (block (drop (f32.eq (i32.const 1))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:149
assert_invalid(
  () => instantiate(`(module (func $$type-binary-num-vs-num-after-return
    (return) (drop (f32.eq (i32.const 1) (f32.const 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:155
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-void-after-return
    (block (return) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:161
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-num-after-return (result i32)
    (block (result i32) (i32.const 1) (return (i32.const 0)) (f32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:167
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-num-vs-void-after-return
    (block (loop (return) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:173
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-num-vs-num-after-return (result i32)
    (loop (result i32) (return (i32.const 1)) (f32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:179
assert_invalid(
  () => instantiate(`(module (func $$type-func-value-num-vs-void-after-return
    (return) (i32.const 1)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:185
assert_invalid(
  () => instantiate(`(module (func $$type-func-value-num-vs-num-after-return (result i32)
    (return (i32.const 1)) (f32.const 0)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:192
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-void-after-unreachable
    (unreachable) (block (drop (i32.eqz (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:198
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-void-in-loop-after-unreachable
    (unreachable) (loop (drop (i32.eqz (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:204
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-void-in-i32-loop-after-unreachable
    (unreachable) (loop (result i32) (i32.eqz (nop)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:210
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-num-after-unreachable
    (unreachable) (drop (i32.eqz (f32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:216
assert_invalid(
  () => instantiate(`(module (func $$type-binary-num-vs-void-after-unreachable
    (unreachable) (block (drop (f32.eq (i32.const 1))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:222
assert_invalid(
  () => instantiate(`(module (func $$type-binary-num-vs-num-after-unreachable
    (unreachable) (drop (f32.eq (i32.const 1) (f32.const 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:228
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-void-after-unreachable
    (block (unreachable) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:234
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-num-after-unreachable (result i32)
    (block (result i32) (i32.const 1) (unreachable) (f32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:240
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-num-vs-void-after-unreachable
    (block (loop (unreachable) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:246
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-num-vs-num-after-unreachable (result i32)
    (loop (result i32) (unreachable) (f32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:252
assert_invalid(
  () => instantiate(`(module (func $$type-func-value-num-vs-void-after-unreachable
    (unreachable) (i32.const 1)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:258
assert_invalid(
  () => instantiate(`(module (func $$type-func-value-num-vs-num-after-unreachable (result i32)
    (unreachable) (f32.const 0)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:264
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-void-in-if-after-unreachable
    (unreachable) (if (i32.const 0) (then (drop (i32.eqz (nop)))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:270
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-void-in-else-after-unreachable
    (unreachable) (if (i32.const 0) (then (nop)) (else (drop (i32.eqz (nop)))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:276
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-void-in-else-after-unreachable-if
    (if (i32.const 0) (then (unreachable)) (else (drop (i32.eqz (nop)))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:283
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-void-after-nested-unreachable
    (block (unreachable)) (block (drop (i32.eqz (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:289
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-num-after-nested-unreachable
    (block (unreachable)) (drop (i32.eqz (f32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:295
assert_invalid(
  () => instantiate(`(module (func $$type-binary-num-vs-void-after-nested-unreachable
    (block (unreachable)) (block (drop (f32.eq (i32.const 1))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:301
assert_invalid(
  () => instantiate(`(module (func $$type-binary-num-vs-num-after-nested-unreachable
    (block (unreachable)) (drop (f32.eq (i32.const 1) (f32.const 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:307
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-void-after-nested-unreachable
    (block (block (unreachable)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:313
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-num-after-nested-unreachable
    (result i32)
    (block (result i32) (i32.const 1) (block (unreachable)) (f32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:320
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-num-vs-void-after-nested-unreachable
    (block (loop (block (unreachable)) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:326
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-num-vs-num-after-nested-unreachable
    (result i32)
    (loop (result i32) (block (unreachable)) (f32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:333
assert_invalid(
  () => instantiate(`(module (func $$type-func-value-num-vs-void-after-nested-unreachable
    (block (unreachable)) (i32.const 1)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:339
assert_invalid(
  () => instantiate(`(module (func $$type-func-value-num-vs-num-after-nested-unreachable
    (result i32)
    (block (unreachable)) (f32.const 0)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:347
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-void-after-infinite-loop
    (loop (br 0)) (block (drop (i32.eqz (nop))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:353
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-num-after-infinite-loop
    (loop (br 0)) (drop (i32.eqz (f32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:359
assert_invalid(
  () => instantiate(`(module (func $$type-binary-num-vs-void-after-infinite-loop
    (loop (br 0)) (block (drop (f32.eq (i32.const 1))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:365
assert_invalid(
  () => instantiate(`(module (func $$type-binary-num-vs-num-after-infinite-loop
    (loop (br 0)) (drop (f32.eq (i32.const 1) (f32.const 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:371
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-void-after-infinite-loop
    (block (loop (br 0)) (i32.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:377
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-num-after-infinite-loop (result i32)
    (block (result i32) (i32.const 1) (loop (br 0)) (f32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:383
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-num-vs-void-after-infinite-loop
    (block (loop (loop (br 0)) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:389
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-num-vs-num-after-infinite-loop (result i32)
    (loop (result i32) (loop (br 0)) (f32.const 0))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:395
assert_invalid(
  () => instantiate(`(module (func $$type-func-value-num-vs-void-after-infinite-loop
    (loop (br 0)) (i32.const 1)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:401
assert_invalid(
  () => instantiate(`(module (func $$type-func-value-num-vs-num-after-infinite-loop (result i32)
    (loop (br 0)) (f32.const 0)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:408
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-void-in-dead-body
    (if (i32.const 0) (then (drop (i32.eqz (nop)))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:414
assert_invalid(
  () => instantiate(`(module (func $$type-unary-num-vs-num-in-dead-body
    (if (i32.const 0) (then (drop (i32.eqz (f32.const 1)))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:420
assert_invalid(
  () => instantiate(`(module (func $$type-binary-num-vs-void-in-dead-body
    (if (i32.const 0) (then (drop (f32.eq (i32.const 1)))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:426
assert_invalid(
  () => instantiate(`(module (func $$type-binary-num-vs-num-in-dead-body
    (if (i32.const 0) (then (drop (f32.eq (i32.const 1) (f32.const 0)))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:432
assert_invalid(
  () => instantiate(`(module (func $$type-if-value-num-vs-void-in-dead-body
    (if (i32.const 0) (then (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:438
assert_invalid(
  () => instantiate(`(module (func $$type-if-value-num-vs-num-in-dead-body (result i32)
    (if (result i32) (i32.const 0) (then (f32.const 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:444
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-void-in-dead-body
    (if (i32.const 0) (then (block (i32.const 1))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:450
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-num-in-dead-body (result i32)
    (if (result i32) (i32.const 0) (then (block (result i32) (f32.const 0))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:456
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-void-in-dead-body
    (if (i32.const 0) (then (loop (i32.const 1))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:462
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-num-vs-num-in-dead-body (result i32)
    (if (result i32) (i32.const 0) (then (loop (result i32) (f32.const 0))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:469
assert_invalid(
  () => instantiate(`(module (func $$type-return-second-num-vs-num (result i32)
    (return (i32.const 1)) (return (f64.const 1))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:476
assert_invalid(
  () => instantiate(`(module (func $$type-br-second-num-vs-num (result i32)
    (block (result i32) (br 0 (i32.const 1)) (br 0 (f64.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:483
assert_invalid(
  () => instantiate(`(module (func $$type-br_if-cond-num-vs-num-after-unreachable
    (block (br_if 0 (unreachable) (f32.const 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:489
assert_invalid(
  () => instantiate(`(module (func $$type-br_if-num-vs-void-after-unreachable (result i32)
    (block (result i32)
      (block (unreachable) (br_if 1 (i32.const 0) (i32.const 0)))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:497
assert_invalid(
  () => instantiate(`(module (func $$type-br_if-num-vs-num-after-unreachable (result i32)
    (block (result i32)
      (block (result f32) (unreachable) (br_if 1 (i32.const 0) (i32.const 0)))
      (drop) (i32.const 0)
    )
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:506
assert_invalid(
  () => instantiate(`(module (func $$type-br_if-num2-vs-num-after-unreachable (result i32)
    (block (result i32)
      (unreachable) (br_if 0 (i32.const 0) (i32.const 0)) (i32.const 0)
    )
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:514
assert_invalid(
  () => instantiate(`(module (func $$type-br_table-num-vs-num-after-unreachable
    (block (br_table 0 (unreachable) (f32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:520
assert_invalid(
  () => instantiate(`(module (func $$type-br_table-label-num-vs-num-after-unreachable (result i32)
    (block (result i32) (unreachable) (br_table 0 (f32.const 0) (i32.const 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:526
assert_invalid(
  () => instantiate(`(module (func $$type-br_table-label-num-vs-label-void-after-unreachable
    (block
      (block (result f32)
        (unreachable)
        (br_table 0 1 0 (i32.const 1))
      )
      (drop)
    )
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:539
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested-unreachable-num-vs-void
    (block (i32.const 3) (block (unreachable)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:545
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested-unreachable-void-vs-num (result i32)
    (block (block (unreachable)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:551
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested-unreachable-num-vs-num (result i32)
    (block (result i64) (i64.const 0) (block (unreachable)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:557
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested-unreachable-num2-vs-void (result i32)
    (block (i32.const 3) (block (i64.const 1) (unreachable))) (i32.const 9)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:564
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested-br-num-vs-void
    (block (i32.const 3) (block (br 1)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:570
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested-br-void-vs-num (result i32)
    (block (result i32) (block (br 1 (i32.const 0))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:576
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested-br-num-vs-num (result i32)
    (block (result i32) (i64.const 0) (block (br 1 (i32.const 0))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:583
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested2-br-num-vs-void
    (block (block (i32.const 3) (block (br 2))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:589
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested2-br-void-vs-num (result i32)
    (block (result i32) (block (block (br 2 (i32.const 0)))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:595
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested2-br-num-vs-num (result i32)
    (block (result i32)
      (block (result i64) (i64.const 0) (block (br 2 (i32.const 0))))
    )
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:603
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested2-br-num2-vs-void (result i32)
    (block (i32.const 3) (block (i64.const 1) (br 1))) (i32.const 9)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:610
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested-return-num-vs-void
    (block (i32.const 3) (block (return)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:616
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested-return-void-vs-num (result i32)
    (block (block (return (i32.const 0))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:622
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested-return-num-vs-num (result i32)
    (block (result i64) (i64.const 0) (block (return (i32.const 0))))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:628
assert_invalid(
  () => instantiate(`(module (func $$type-block-value-nested-return-num2-vs-void (result i32)
    (block (i32.const 3) (block (i64.const 1) (return (i32.const 0))))
    (i32.const 9)
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:636
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-nested-unreachable-num-vs-void
    (loop (i32.const 3) (block (unreachable)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:642
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-nested-unreachable-void-vs-num (result i32)
    (loop (block (unreachable)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:648
assert_invalid(
  () => instantiate(`(module (func $$type-loop-value-nested-unreachable-num-vs-num (result i32)
    (loop (result i64) (i64.const 0) (block (unreachable)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:655
assert_invalid(
  () => instantiate(`(module (func $$type-cont-last-void-vs-empty (result i32)
    (loop (br 0 (nop)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:661
assert_invalid(
  () => instantiate(`(module (func $$type-cont-last-num-vs-empty (result i32)
    (loop (br 0 (i32.const 0)))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:668
assert_invalid(
  () => instantiate(`(module (func $$tee-local-unreachable-value
    (local i32)
    (local.tee 0 (unreachable))
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:675
assert_invalid(
  () => instantiate(`(module (func $$br_if-unreachable (result i32)
    (block (result i32)
      (block
        (br_if 1 (unreachable) (i32.const 0))
      )
      (i32.const 0)
    )
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:686
assert_invalid(
  () => instantiate(`(module
    (func $$type-br_if-after-unreachable (result i64)
      (unreachable)
      (br_if 0)
      (i64.extend_i32_u)
    )
  )`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:697
assert_invalid(
  () => instantiate(`(module
    (func $$type-after-ref.as_non_null
      (unreachable)
      (ref.as_non_null)
      (f32.abs)
    )
  )`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:709
assert_invalid(
  () => instantiate(`(module (func (unreachable) (select (i32.const 1) (i64.const 1) (i32.const 1)) (drop)))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:714
assert_invalid(
  () => instantiate(`(module (func (unreachable) (select (i64.const 1) (i32.const 1) (i32.const 1)) (drop)))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:720
assert_invalid(
  () => instantiate(`(module (func (unreachable) (select (i32.const 1) (i32.const 1) (i64.const 1)) (drop)))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:725
assert_invalid(
  () => instantiate(`(module (func (unreachable) (select (i32.const 1) (i64.const 1)) (drop)))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:730
assert_invalid(
  () => instantiate(`(module (func (unreachable) (select (i64.const 1)) (drop)))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:736
assert_invalid(
  () => instantiate(`(module (func (result i32) (unreachable) (select (i64.const 1) (i32.const 1))))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:743
assert_invalid(
  () => instantiate(`(module (func (unreachable) (select)))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:748
assert_invalid(
  () => instantiate(`(module (func $$meet-bottom (param i32) (result externref)
    (block $$l1 (result externref)
      (drop
        (block $$l2 (result i32)
          (br_table $$l2 $$l1 $$l2 (ref.null extern) (local.get 0))
        )
      )
      (ref.null extern)
    )
  ))`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:763
assert_invalid(
  () => instantiate(`(module
    (type $$t (func (param i32) (result i64)))
    (func (result i32)
      (unreachable)
      (call_ref $$t)
    )
  )`),
  `type mismatch`,
);

// ./test/core/unreached-invalid.wast:773
assert_invalid(
  () => instantiate(`(module
    (type $$t (func (param i32) (result i32 i32)))
    (func (result i32)
      (unreachable)
      (call_ref $$t)
    )
  )`),
  `type mismatch`,
);
