// |jit-test| test-also=--setpref=wasm_lazy_tiering --setpref=wasm_lazy_tiering_synchronous; skip-if: wasmCompileMode() != "baseline+ion" || !getPrefValue("wasm_lazy_tiering")

// Tests the inliner on a recursive function, in particular to establish that
// the inlining heuristics have some way to stop the compiler looping
// indefinitely and, more constrainingly, that it has some way to stop
// excessive but finite inlining.

// `func $recursive` has 95 bytecode bytes.  With module- and function-level
// inlining budgeting disabled, it is inlined into itself 1110 times,
// processing an extra 105450 bytecode bytes.  This is definitely excessive.
//
// With budgeting re-enabled, it is inlined just 9 times, as intended.

let t = `
(module
  (func $recursive (export "recursive") (param i32) (result i32)
    (i32.le_u (local.get 0) (i32.const 1))
    if (result i32)
      (i32.const 1)
    else
      (i32.const 1)

      (call $recursive (i32.sub (local.get 0) (i32.const 1)))
      i32.add
      (call $recursive (i32.sub (local.get 0) (i32.const 2)))
      i32.add

      (call $recursive (i32.sub (local.get 0) (i32.const 1)))
      i32.add
      (call $recursive (i32.sub (local.get 0) (i32.const 2)))
      i32.add

      (call $recursive (i32.sub (local.get 0) (i32.const 1)))
      i32.add
      (call $recursive (i32.sub (local.get 0) (i32.const 2)))
      i32.add

      (call $recursive (i32.sub (local.get 0) (i32.const 1)))
      i32.add
      (call $recursive (i32.sub (local.get 0) (i32.const 2)))
      i32.add

      (call $recursive (i32.sub (local.get 0) (i32.const 1)))
      i32.add
      (call $recursive (i32.sub (local.get 0) (i32.const 2)))
      i32.add
    end
  )
)`;

let i = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(t)));

assertEq(i.exports.recursive(10), 14517361);

// This assertion will fail if there is runaway recursion, because the
// optimised compilation will run long and hence not be completed by the time
// the assertion is evaluated.
assertEq(wasmFunctionTier(i.exports.recursive), "optimized");
