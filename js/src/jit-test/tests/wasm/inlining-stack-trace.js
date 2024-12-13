// |jit-test| -P wasm_lazy_tiering; -P wasm_lazy_tiering_synchronous; -P wasm_lazy_tiering_level=9; -P wasm_inlining_level=9; skip-if: wasmCompileMode() != "baseline+ion" || !getPrefValue("wasm_lazy_tiering")

let {assertStackTrace} = WasmHelpers;

let exports = wasmEvalText(`(module
  (func $throw3 (import "" "throw3"))

  (memory 1 1)
  (type $s (struct (field i32)))


  (func $throw2
    call $throw3
  )
  (func $throw1 (export "throw1")
    call $throw2
  )

  (func $unreachable2
    unreachable
  )
  (func $unreachable1 (export "unreachable1")
    call $unreachable2
  )

  (func $oob2
    i32.const -1
    i32.load
    drop
  )
  (func $oob1 (export "oob1")
    call $oob2
  )

  (func $null2
    ref.null $s
    struct.get $s 0
    drop
  )
  (func $null1 (export "null1")
    call $null2
  )

  (func $div2
    i32.const 1
    i32.const 0
    i32.div_u
    drop
  )
  (func $div1 (export "div1")
    call $div2
  )

  (func $mod2
    i32.const 1
    i32.const 0
    i32.rem_u
    drop
  )
  (func $mod1 (export "mod1")
    call $mod2
  )
)`, {"": {throw3: () => { throw new Error() }}}).exports;

let tests = [
  {func: exports.throw1, stack: ['throw3', 'throw2', 'throw1', '']},
  {func: exports.unreachable1, stack: ['unreachable2', 'unreachable1', '']},
  {func: exports.oob1, stack: ['oob2', 'oob1', '']},
  {func: exports.null1, stack: ['null2', 'null1', '']},
  {func: exports.div1, stack: ['div2', 'div1', '']},
  {func: exports.mod1, stack: ['mod2', 'mod1', '']},
];

// Run this two times. The first time should trigger a synchronous compile,
// and the second time should have inlining.
for (let i = 1; i <= 2; i++) {
  for (let {func, stack} of tests) {
    if (i === 2) {
      assertEq(wasmFunctionTier(func), "optimized");
    }

    let success = false;
    try {
      func();
      success = true;
    } catch (e) {
      assertStackTrace(e, stack);
    }

    // The above should always throw an exception
    assertEq(success, false);
  }
}
