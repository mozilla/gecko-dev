// |jit-test| test-also=--setpref=wasm_lazy_tiering --setpref=wasm_lazy_tiering_synchronous; skip-if: wasmCompileMode() != "baseline+ion" || !getPrefValue("wasm_lazy_tiering")

// Needs to be at least 13500 in order for test functions to tier up.
// See Instance::computeInitialHotnessCounter.
const tierUpThreshold = 14000;

let {importFunc} = wasmEvalText(`
  (module (func (export "importFunc") (result i32) i32.const 2))
`).exports;
let testFuncs = [
  [importFunc, 2],
  ["trueFunc", 1],
  ["falseFunc", 0],
  ["trapFunc", WebAssembly.RuntimeError],
  [null, WebAssembly.RuntimeError],
];
function invokeTestWith(exports, exportThing, expected) {
  let targetFunc;
  if (exportThing instanceof Function) {
    targetFunc = exportThing;
  } else if (exportThing === null) {
    targetFunc = null;
  } else {
    targetFunc = exports[exportThing];
  }

  if (expected === WebAssembly.RuntimeError) {
    assertErrorMessage(() => exports.test(targetFunc), WebAssembly.RuntimeError, /./);
  } else {
    assertEq(exports.test(targetFunc), expected);
  }
}

for ([funcToInline, funcToInlineExpected] of testFuncs) {
  let exports = wasmEvalText(`
  (module
    (type $booleanFunc (func (result i32)))

    (func $importFunc (import "" "importFunc") (result i32))
    (func $trueFunc (export "trueFunc") (result i32)
      i32.const 1
    )
    (func $falseFunc (export "falseFunc") (result i32)
      i32.const 0
    )
    (func $trapFunc (export "trapFunc") (result i32)
      unreachable
    )

    (func (export "test") (param (ref null $booleanFunc)) (result i32)
      local.get 0
      call_ref $booleanFunc
    )
  )`, {"": {importFunc}}).exports;
  let test = exports["test"];

  // Force a tier-up of the function, calling the same function every time
  assertEq(wasmFunctionTier(test), "baseline");
  for (let i = 0; i <= tierUpThreshold; i++) {
    invokeTestWith(exports, funcToInline, funcToInlineExpected);
  }
  assertEq(wasmFunctionTier(test), "optimized");

  // Now that we've inlined something, try calling it with every test function
  // and double check we get the expected behavior
  for ([testFunc, testFuncExpected] of testFuncs) {
    invokeTestWith(exports, testFunc, testFuncExpected);
  }
}
