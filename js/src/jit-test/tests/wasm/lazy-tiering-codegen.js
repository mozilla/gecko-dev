// |jit-test| skip-if: !wasmExperimentalCompilePipelineEnabled() || !getBuildConfiguration("x64") || !hasDisassembler() || getBuildConfiguration("simulator"); test-also=-P wasm_experimental_compile_pipeline; include:codegen-x64-test.js

assertEq(hasDisassembler(), true);

// Test the compiled result of this function, but be very careful not to run
// it, because we don't want it to tier-up.  This is close to being the
// smallest wasm function whose entry-point tier-up check will have a down-step
// of 3, which is what we check for below.
let t = `
 (module
   (func $f (export "f") (param i32) (result i32)
      (local.get 0)
      (i32.const 1)
      i32.add
      (i32.const 2)
      i32.and
      (i32.const 3)
      i32.or
      (i32.const 4)
      i32.xor
      (i32.const 5)
      i32.add
      (i32.const 6)
      i32.and
      (i32.const 7)
      i32.or
      (i32.const 8)
      i32.xor
      (i32.const 9)
      i32.add
      (i32.const 10)
      i32.and
      (i32.const 11)
      i32.or
      (i32.const 12)
      i32.xor
      (i32.const 13)
      i32.add
      (i32.const 14)
      i32.and
      (i32.const 15)
      i32.or
      (i32.const 16)
      i32.xor
      (i32.const 17)
      i32.add
      (i32.const 18)
      i32.and
      (i32.const 19)
      i32.or
   )
 )
`;

let expected =
`41 83 ae .. 0. 00 00 03   subl \\$0x03, 0x...\\(%r14\\)
 0f 88 .. .. 00 00         js 0x000000000000....`;

codegenTestX64_adhoc(t, "f", expected,
                     {no_prefix:true, no_suffix:true, baseline:true});
