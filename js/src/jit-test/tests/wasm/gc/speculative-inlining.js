// |jit-test| skip-if: !wasmGcEnabled(); test-also=-P wasm_experimental_compile_pipeline;

// Basic test that call_ref profiling information works. Will be expanded in
// a future commit.
let {test} = wasmEvalText(`
(module
  (type $refType (func (result i32)))
  (func $ref (export "ref") (result i32)
    i32.const 1
  )
  (func (export "test") (result i32)
    ref.func $ref
    call_ref $refType
  )
)`).exports;

for (let i = 0; i < 10; i++) {
  assertEq(test(), 1);
}
