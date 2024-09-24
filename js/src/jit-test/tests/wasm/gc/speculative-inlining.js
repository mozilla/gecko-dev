// |jit-test| skip-if: !wasmGcEnabled() || !wasmExperimentalCompilePipelineEnabled(); test-also=-P wasm_experimental_compile_pipeline;

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
  // Give the off-thread Ion compilation a chance to catch up.  This is really
  // a kludge in that we currently have no reliable way in JS to wait for all
  // requested tier-ups to complete.  Better would be to put the sleep and
  // assertEq inside an exponential backoff loop and have that as a "library"
  // function.
  sleep(0.05);
  assertEq(wasmFunctionTier(test), "optimized");

  // Now that we've inlined something, try calling it with every test function
  // and double check we get the expected behavior
  for ([testFunc, testFuncExpected] of testFuncs) {
    invokeTestWith(exports, testFunc, testFuncExpected);
  }
}
