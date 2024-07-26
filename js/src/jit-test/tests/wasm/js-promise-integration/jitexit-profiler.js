// |jit-test| skip-if: !WasmHelpers.isSingleStepProfilingEnabled

// Copy of wasm/import-export.js test with WebAssembly.promising

(async function testImportJitExit() {
  let options = getJitCompilerOptions();
  if (!options['baseline.enable'])
      return;

  let baselineTrigger = options['baseline.warmup.trigger'];

  let valueToConvert = 0;
  function ffi(n) { if (n == 1337) { return valueToConvert }; return 42; }

  function sum(a, b, c) {
      if (a === 1337)
          return valueToConvert;
      return (a|0) + (b|0) + (c|0) | 0;
  }

  // Baseline compile ffis.
  for (let i = baselineTrigger + 1; i --> 0;) {
      ffi(i);
      sum((i%2)?i:undefined,
          (i%3)?i:undefined,
          (i%4)?i:undefined);
  }

  let imports = {
      a: {
          ffi,
          sum
      }
  };

  i = wasmEvalText(`(module
      (import "a" "ffi" (func $ffi (param i32) (result i32)))

      (import "a" "sum" (func $missingOneArg (param i32) (param i32) (result i32)))
      (import "a" "sum" (func $missingTwoArgs (param i32) (result i32)))
      (import "a" "sum" (func $missingThreeArgs (result i32)))

      (func (export "foo") (param i32) (result i32)
       local.get 0
       call $ffi
      )

      (func (export "missThree") (result i32)
       call $missingThreeArgs
      )

      (func (export "missTwo") (param i32) (result i32)
       local.get 0
       call $missingTwoArgs
      )

      (func (export "missOne") (param i32) (param i32) (result i32)
       local.get 0
       local.get 1
       call $missingOneArg
      )
  )`, imports).exports;

  // Enable the jit exit for each JS callee.
  assertEq(i.foo(0), 42);

  assertEq(i.missThree(), 0);
  assertEq(i.missTwo(42), 42);
  assertEq(i.missOne(13, 37), 50);

  // Test the jit exit under normal conditions.
  assertEq(i.foo(0), 42);
  assertEq(i.foo(1337), 0);

  enableGeckoProfiling();
  enableSingleStepProfiling();

  // Test on suspendable stack.
  var f = WebAssembly.promising(i.foo);
  assertEq(await f(0), 42);

  const stacks = disableSingleStepProfiling();
  WasmHelpers.assertEqImpreciseStacks(stacks,
    ["", ">", "1,>", "<,1,>", "CreateSuspender,1,>", "<,1,>", "1,>", "<,1,>",
     "CreatePromisingPromise,1,>", "<,1,>", "1,>", "<,1,>", "#ref.func function,1,>",
     "<,1,>", "1,>", "<,1,>", "#update suspender state util,1,>", "<,1,>", "1,>",
     "2,1,>", "4,2,1,>", "<,4,2,1,>", "4,2,1,>", "2,1,>", "<,2,1,>",
     "SetPromisingPromiseResults,2,1,>", "<,2,1,>", "2,1,>", "1,>", "<,1,>",
     "#update suspender state util,1,>", "<,1,>", "1,>", ">", ""]
  );

  disableGeckoProfiling();

  // Test the arguments rectifier.
  assertEq(i.missThree(), 0);
  assertEq(i.missTwo(-1), -1);
  assertEq(i.missOne(23, 10), 33);

  // Test OOL coercion.
  valueToConvert = 2**31;
  assertEq(i.foo(1337), -(2**31));

  // Test OOL error path.
  valueToConvert = { valueOf() { throw new Error('make ffi great again'); } }
  assertErrorMessage(() => i.foo(1337), Error, "make ffi great again");

  valueToConvert = { toString() { throw new Error('a FFI to believe in'); } }
  assertErrorMessage(() => i.foo(1337), Error, "a FFI to believe in");

  // Test the error path in the arguments rectifier.
  assertErrorMessage(() => i.missTwo(1337), Error, "a FFI to believe in");
})();
