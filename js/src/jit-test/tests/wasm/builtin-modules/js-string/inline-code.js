// |jit-test| skip-if: !wasmJSStringBuiltinsEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("arm64"); include:codegen-arm64-test.js

codegenTestARM64_adhoc(`
  (func $testImp
    (import "wasm:js-string" "test")
    (param externref)
    (result i32)
  )
  (export "test" (func $testImp))`,
  'test',
  `aa0003e1  mov     x1, x0
  92400421  and     x1, x1, #0x3
  7100083f  cmp     w1, #0x2 \\(2\\)
  54000060  b\\.eq    #\\+0xc \\(addr .*\\)
  52800000  mov     w0, #0x0
  14000002  b       #\\+0x8 \\(addr .*\\)
  52800020  mov     w0, #0x1`,
  {features: {builtins: ["js-string"]}}
);

codegenTestARM64_adhoc(`
  (func $castImp
    (import "wasm:js-string" "cast")
    (param externref)
    (result (ref extern))
  )
  (export "cast" (func $castImp))`,
  'cast',
  `aa0003e1  mov     x1, x0
  92400421  and     x1, x1, #0x3
  7100083f  cmp     w1, #0x2 \\(2\\)
  54000040  b.eq    #\\+0x8 \\(addr .*\\)
  d4a00000  unimplemented \\(Exception\\)`,
  {features: {builtins: ["js-string"]}}
);
