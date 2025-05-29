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

// ./test/core/try_table.wast

// ./test/core/try_table.wast:3
let $0 = instantiate(`(module
  (tag $$e0 (export "e0"))
  (func (export "throw") (throw $$e0))
)`);

// ./test/core/try_table.wast:8
register($0, `test`);

// ./test/core/try_table.wast:10
let $1 = instantiate(`(module
  (tag $$imported-e0 (import "test" "e0"))
  (tag $$imported-e0-alias (import "test" "e0"))
  (func $$imported-throw (import "test" "throw"))
  (tag $$e0)
  (tag $$e1)
  (tag $$e2)
  (tag $$e-i32 (param i32))
  (tag $$e-f32 (param f32))
  (tag $$e-i64 (param i64))
  (tag $$e-f64 (param f64))

  (func $$throw-if (param i32) (result i32)
    (local.get 0)
    (i32.const 0) (if (i32.ne) (then (throw $$e0)))
    (i32.const 0)
  )

  (func (export "simple-throw-catch") (param i32) (result i32)
    (block $$h
      (try_table (result i32) (catch $$e0 $$h)
        (if (i32.eqz (local.get 0)) (then (throw $$e0)) (else))
        (i32.const 42)
      )
      (return)
    )
    (i32.const 23)
  )

  (func (export "unreachable-not-caught")
    (block $$h
      (try_table (catch_all $$h) (unreachable))
      (return)
    )
  )

  (func $$div (param i32 i32) (result i32)
    (local.get 0) (local.get 1) (i32.div_u)
  )
  (func (export "trap-in-callee") (param i32 i32) (result i32)
    (block $$h
      (try_table (result i32) (catch_all $$h)
        (call $$div (local.get 0) (local.get 1))
      )
      (return)
    )
    (i32.const 11)
  )

  (func (export "catch-complex-1") (param i32) (result i32)
    (block $$h1
      (try_table (result i32) (catch $$e1 $$h1)
        (block $$h0
          (try_table (result i32) (catch $$e0 $$h0)
            (if (i32.eqz (local.get 0))
              (then (throw $$e0))
              (else
                (if (i32.eq (local.get 0) (i32.const 1))
                  (then (throw $$e1))
                  (else (throw $$e2))
                )
              )
            )
            (i32.const 2)
          )
          (br 1)
        )
        (i32.const 3)
      )
      (return)
    )
    (i32.const 4)
  )

  (func (export "catch-complex-2") (param i32) (result i32)
    (block $$h0
      (block $$h1
        (try_table (result i32) (catch $$e0 $$h0) (catch $$e1 $$h1)
          (if (i32.eqz (local.get 0))
            (then (throw $$e0))
            (else
              (if (i32.eq (local.get 0) (i32.const 1))
                (then (throw $$e1))
                (else (throw $$e2))
              )
            )
           )
          (i32.const 2)
        )
        (return)
      )
      (return (i32.const 4))
    )
    (i32.const 3)
  )

  (func (export "throw-catch-param-i32") (param i32) (result i32)
    (block $$h (result i32)
      (try_table (result i32) (catch $$e-i32 $$h)
        (throw $$e-i32 (local.get 0))
        (i32.const 2)
      )
      (return)
    )
    (return)
  )

  (func (export "throw-catch-param-f32") (param f32) (result f32)
    (block $$h (result f32)
      (try_table (result f32) (catch $$e-f32 $$h)
        (throw $$e-f32 (local.get 0))
        (f32.const 0)
      )
      (return)
    )
    (return)
  )

  (func (export "throw-catch-param-i64") (param i64) (result i64)
    (block $$h (result i64)
      (try_table (result i64) (catch $$e-i64 $$h)
        (throw $$e-i64 (local.get 0))
        (i64.const 2)
      )
      (return)
    )
    (return)
  )

  (func (export "throw-catch-param-f64") (param f64) (result f64)
    (block $$h (result f64)
      (try_table (result f64) (catch $$e-f64 $$h)
        (throw $$e-f64 (local.get 0))
        (f64.const 0)
      )
      (return)
    )
    (return)
  )

  (func (export "throw-catch_ref-param-i32") (param i32) (result i32)
    (block $$h (result i32 exnref)
      (try_table (result i32) (catch_ref $$e-i32 $$h)
        (throw $$e-i32 (local.get 0))
        (i32.const 2)
      )
      (return)
    )
    (drop) (return)
  )

  (func (export "throw-catch_ref-param-f32") (param f32) (result f32)
    (block $$h (result f32 exnref)
      (try_table (result f32) (catch_ref $$e-f32 $$h)
        (throw $$e-f32 (local.get 0))
        (f32.const 0)
      )
      (return)
    )
    (drop) (return)
  )

  (func (export "throw-catch_ref-param-i64") (param i64) (result i64)
    (block $$h (result i64 exnref)
      (try_table (result i64) (catch_ref $$e-i64 $$h)
        (throw $$e-i64 (local.get 0))
        (i64.const 2)
      )
      (return)
    )
    (drop) (return)
  )

  (func (export "throw-catch_ref-param-f64") (param f64) (result f64)
    (block $$h (result f64 exnref)
      (try_table (result f64) (catch_ref $$e-f64 $$h)
        (throw $$e-f64 (local.get 0))
        (f64.const 0)
      )
      (return)
    )
    (drop) (return)
  )

  (func $$throw-param-i32 (param i32) (throw $$e-i32 (local.get 0)))
  (func (export "catch-param-i32") (param i32) (result i32)
    (block $$h (result i32)
      (try_table (result i32) (catch $$e-i32 $$h)
        (i32.const 0)
        (call $$throw-param-i32 (local.get 0))
      )
      (return)
    )
  )

  (func (export "catch-imported") (result i32)
    (block $$h
      (try_table (result i32) (catch $$imported-e0 $$h)
        (call $$imported-throw (i32.const 1))
      )
      (return)
    )
    (i32.const 2)
  )

  (func (export "catch-imported-alias") (result i32)
    (block $$h
      (try_table (result i32) (catch $$imported-e0 $$h)
        (throw $$imported-e0-alias (i32.const 1))
      )
      (return)
    )
    (i32.const 2)
  )

  (func (export "catchless-try") (param i32) (result i32)
    (block $$h
      (try_table (result i32) (catch $$e0 $$h)
        (try_table (result i32) (call $$throw-if (local.get 0)))
      )
      (return)
    )
    (i32.const 1)
  )

  (func $$throw-void (throw $$e0))
  (func (export "return-call-in-try-catch")
    (block $$h
      (try_table (catch $$e0 $$h)
        (return_call $$throw-void)
      )
    )
  )

  (table funcref (elem $$throw-void))
  (func (export "return-call-indirect-in-try-catch")
    (block $$h
      (try_table (catch $$e0 $$h)
        (return_call_indirect (i32.const 0))
      )
    )
  )

  (func (export "try-with-param")
    (i32.const 0) (try_table (param i32) (drop))
  )
)`);

// ./test/core/try_table.wast:258
assert_return(() => invoke($1, `simple-throw-catch`, [0]), [value("i32", 23)]);

// ./test/core/try_table.wast:259
assert_return(() => invoke($1, `simple-throw-catch`, [1]), [value("i32", 42)]);

// ./test/core/try_table.wast:261
assert_trap(() => invoke($1, `unreachable-not-caught`, []), `unreachable`);

// ./test/core/try_table.wast:263
assert_return(() => invoke($1, `trap-in-callee`, [7, 2]), [value("i32", 3)]);

// ./test/core/try_table.wast:264
assert_trap(() => invoke($1, `trap-in-callee`, [1, 0]), `integer divide by zero`);

// ./test/core/try_table.wast:266
assert_return(() => invoke($1, `catch-complex-1`, [0]), [value("i32", 3)]);

// ./test/core/try_table.wast:267
assert_return(() => invoke($1, `catch-complex-1`, [1]), [value("i32", 4)]);

// ./test/core/try_table.wast:268
assert_exception(() => invoke($1, `catch-complex-1`, [2]));

// ./test/core/try_table.wast:270
assert_return(() => invoke($1, `catch-complex-2`, [0]), [value("i32", 3)]);

// ./test/core/try_table.wast:271
assert_return(() => invoke($1, `catch-complex-2`, [1]), [value("i32", 4)]);

// ./test/core/try_table.wast:272
assert_exception(() => invoke($1, `catch-complex-2`, [2]));

// ./test/core/try_table.wast:274
assert_return(() => invoke($1, `throw-catch-param-i32`, [0]), [value("i32", 0)]);

// ./test/core/try_table.wast:275
assert_return(() => invoke($1, `throw-catch-param-i32`, [1]), [value("i32", 1)]);

// ./test/core/try_table.wast:276
assert_return(() => invoke($1, `throw-catch-param-i32`, [10]), [value("i32", 10)]);

// ./test/core/try_table.wast:278
assert_return(() => invoke($1, `throw-catch-param-f32`, [value("f32", 5)]), [value("f32", 5)]);

// ./test/core/try_table.wast:279
assert_return(() => invoke($1, `throw-catch-param-f32`, [value("f32", 10.5)]), [value("f32", 10.5)]);

// ./test/core/try_table.wast:281
assert_return(() => invoke($1, `throw-catch-param-i64`, [5n]), [value("i64", 5n)]);

// ./test/core/try_table.wast:282
assert_return(() => invoke($1, `throw-catch-param-i64`, [0n]), [value("i64", 0n)]);

// ./test/core/try_table.wast:283
assert_return(() => invoke($1, `throw-catch-param-i64`, [-1n]), [value("i64", -1n)]);

// ./test/core/try_table.wast:285
assert_return(() => invoke($1, `throw-catch-param-f64`, [value("f64", 5)]), [value("f64", 5)]);

// ./test/core/try_table.wast:286
assert_return(() => invoke($1, `throw-catch-param-f64`, [value("f64", 10.5)]), [value("f64", 10.5)]);

// ./test/core/try_table.wast:288
assert_return(() => invoke($1, `throw-catch_ref-param-i32`, [0]), [value("i32", 0)]);

// ./test/core/try_table.wast:289
assert_return(() => invoke($1, `throw-catch_ref-param-i32`, [1]), [value("i32", 1)]);

// ./test/core/try_table.wast:290
assert_return(() => invoke($1, `throw-catch_ref-param-i32`, [10]), [value("i32", 10)]);

// ./test/core/try_table.wast:292
assert_return(() => invoke($1, `throw-catch_ref-param-f32`, [value("f32", 5)]), [value("f32", 5)]);

// ./test/core/try_table.wast:293
assert_return(() => invoke($1, `throw-catch_ref-param-f32`, [value("f32", 10.5)]), [value("f32", 10.5)]);

// ./test/core/try_table.wast:295
assert_return(() => invoke($1, `throw-catch_ref-param-i64`, [5n]), [value("i64", 5n)]);

// ./test/core/try_table.wast:296
assert_return(() => invoke($1, `throw-catch_ref-param-i64`, [0n]), [value("i64", 0n)]);

// ./test/core/try_table.wast:297
assert_return(() => invoke($1, `throw-catch_ref-param-i64`, [-1n]), [value("i64", -1n)]);

// ./test/core/try_table.wast:299
assert_return(() => invoke($1, `throw-catch_ref-param-f64`, [value("f64", 5)]), [value("f64", 5)]);

// ./test/core/try_table.wast:300
assert_return(() => invoke($1, `throw-catch_ref-param-f64`, [value("f64", 10.5)]), [value("f64", 10.5)]);

// ./test/core/try_table.wast:302
assert_return(() => invoke($1, `catch-param-i32`, [5]), [value("i32", 5)]);

// ./test/core/try_table.wast:304
assert_return(() => invoke($1, `catch-imported`, []), [value("i32", 2)]);

// ./test/core/try_table.wast:305
assert_return(() => invoke($1, `catch-imported-alias`, []), [value("i32", 2)]);

// ./test/core/try_table.wast:307
assert_return(() => invoke($1, `catchless-try`, [0]), [value("i32", 0)]);

// ./test/core/try_table.wast:308
assert_return(() => invoke($1, `catchless-try`, [1]), [value("i32", 1)]);

// ./test/core/try_table.wast:310
assert_exception(() => invoke($1, `return-call-in-try-catch`, []));

// ./test/core/try_table.wast:311
assert_exception(() => invoke($1, `return-call-indirect-in-try-catch`, []));

// ./test/core/try_table.wast:313
assert_return(() => invoke($1, `try-with-param`, []), []);

// ./test/core/try_table.wast:315
let $2 = instantiate(`(module
  (func $$imported-throw (import "test" "throw"))
  (tag $$e0)

  (func (export "imported-mismatch") (result i32)
    (block $$h
      (try_table (result i32) (catch_all $$h)
        (block $$h0
          (try_table (result i32) (catch $$e0 $$h0)
            (i32.const 1)
            (call $$imported-throw)
          )
          (return)
        )
        (i32.const 2)
      )
      (return)
    )
    (i32.const 3)
  )
)`);

// ./test/core/try_table.wast:337
assert_return(() => invoke($2, `imported-mismatch`, []), [value("i32", 3)]);

// ./test/core/try_table.wast:339
assert_malformed(() => instantiate(`(module (func (catch_all))) `), `unexpected token`);

// ./test/core/try_table.wast:344
assert_malformed(
  () => instantiate(`(module (tag $$e) (func (catch $$e))) `),
  `unexpected token`,
);

// ./test/core/try_table.wast:349
let $3 = instantiate(`(module
  (tag $$e)
  (func (try_table (catch $$e 0) (catch $$e 0)))
  (func (try_table (catch_all 0) (catch $$e 0)))
  (func (try_table (catch_all 0) (catch_all 0)))
  (func (result exnref) (try_table (catch_ref $$e 0) (catch_ref $$e 0)) (unreachable))
  (func (result exnref) (try_table (catch_all_ref 0) (catch_ref $$e 0)) (unreachable))
  (func (result exnref) (try_table (catch_all_ref 0) (catch_all_ref 0)) (unreachable))
)`);

// ./test/core/try_table.wast:359
assert_invalid(
  () => instantiate(`(module (func (result i32) (try_table (result i32))))`),
  `type mismatch`,
);

// ./test/core/try_table.wast:363
assert_invalid(
  () => instantiate(`(module (func (result i32) (try_table (result i32) (i64.const 42))))`),
  `type mismatch`,
);

// ./test/core/try_table.wast:368
assert_invalid(
  () => instantiate(`(module (tag) (func (try_table (catch_ref 0 0))))`),
  `type mismatch`,
);

// ./test/core/try_table.wast:372
assert_invalid(
  () => instantiate(`(module (tag) (func (result exnref) (try_table (catch 0 0)) (unreachable)))`),
  `type mismatch`,
);

// ./test/core/try_table.wast:376
assert_invalid(
  () => instantiate(`(module (func (try_table (catch_all_ref 0))))`),
  `type mismatch`,
);

// ./test/core/try_table.wast:380
assert_invalid(
  () => instantiate(`(module (func (result exnref) (try_table (catch_all 0)) (unreachable)))`),
  `type mismatch`,
);

// ./test/core/try_table.wast:384
assert_invalid(
  () => instantiate(`(module
    (tag (param i64))
    (func (result i32 exnref) (try_table (result i32) (catch_ref 0 0) (i32.const 42)))
  )`),
  `type mismatch`,
);

// ./test/core/try_table.wast:393
let $4 = instantiate(`(module
  (type $$t (func))
  (func $$dummy)
  (elem declare func $$dummy)

  (tag $$e (param (ref $$t)))
  (func $$throw (throw $$e (ref.func $$dummy)))

  (func (export "catch") (result (ref null $$t))
    (block $$l (result (ref null $$t))
      (try_table (catch $$e $$l) (call $$throw))
      (unreachable)
    )
  )
  (func (export "catch_ref1") (result (ref null $$t))
    (block $$l (result (ref null $$t) (ref exn))
      (try_table (catch_ref $$e $$l) (call $$throw))
      (unreachable)
    )
    (drop)
  )
  (func (export "catch_ref2") (result (ref null $$t))
    (block $$l (result (ref null $$t) (ref null exn))
      (try_table (catch_ref $$e $$l) (call $$throw))
      (unreachable)
    )
    (drop)
  )
  (func (export "catch_all_ref1")
    (block $$l (result (ref exn))
      (try_table (catch_all_ref $$l) (call $$throw))
      (unreachable)
    )
    (drop)
  )
  (func (export "catch_all_ref2")
    (block $$l (result (ref null exn))
      (try_table (catch_all_ref $$l) (call $$throw))
      (unreachable)
    )
    (drop)
  )
)`);

// ./test/core/try_table.wast:437
assert_return(() => invoke($4, `catch`, []), [new RefWithType('funcref')]);

// ./test/core/try_table.wast:438
assert_return(() => invoke($4, `catch_ref1`, []), [new RefWithType('funcref')]);

// ./test/core/try_table.wast:439
assert_return(() => invoke($4, `catch_ref2`, []), [new RefWithType('funcref')]);

// ./test/core/try_table.wast:440
assert_return(() => invoke($4, `catch_all_ref1`, []), []);

// ./test/core/try_table.wast:441
assert_return(() => invoke($4, `catch_all_ref2`, []), []);

// ./test/core/try_table.wast:443
assert_invalid(
  () => instantiate(`(module
    (type $$t (func))
    (tag $$e (param (ref null $$t)))
    (func (export "catch") (result (ref $$t))
      (block $$l (result (ref $$t))
        (try_table (catch $$e $$l))
        (unreachable)
      )
    )
  )`),
  `type mismatch`,
);

// ./test/core/try_table.wast:456
assert_invalid(
  () => instantiate(`(module
    (type $$t (func))
    (tag $$e (param (ref null $$t)))
    (func (export "catch_ref") (result (ref $$t))
      (block $$l (result (ref $$t) (ref exn))
        (try_table (catch $$e $$l))
        (unreachable)
      )
    )
  )`),
  `type mismatch`,
);
