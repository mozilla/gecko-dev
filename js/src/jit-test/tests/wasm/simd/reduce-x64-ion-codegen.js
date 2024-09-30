// |jit-test| skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("x64") || getBuildConfiguration("simulator") || isAvxPresent(); include:codegen-x64-test.js

// Test encoding of the all_true, and any_true operations.

codegenTestX64_v128_i32(
     [['v128.any_true', `
33 c0                     xor %eax, %eax
66 0f 38 17 c0            ptest %xmm0, %xmm0
0f 95 c0                  setnz %al`],
     ['i8x16.all_true', `
33 c0                     xor %eax, %eax
66 45 0f ef ff            pxor %xmm15, %xmm15
66 44 0f 74 f8            pcmpeqb %xmm0, %xmm15
66 45 0f 38 17 ff         ptest %xmm15, %xmm15
0f 94 c0                  setz %al`],
     ['i16x8.all_true', `
33 c0                     xor %eax, %eax
66 45 0f ef ff            pxor %xmm15, %xmm15
66 44 0f 75 f8            pcmpeqw %xmm0, %xmm15
66 45 0f 38 17 ff         ptest %xmm15, %xmm15
0f 94 c0                  setz %al`],
     ['i32x4.all_true', `
33 c0                     xor %eax, %eax
66 45 0f ef ff            pxor %xmm15, %xmm15
66 44 0f 76 f8            pcmpeqd %xmm0, %xmm15
66 45 0f 38 17 ff         ptest %xmm15, %xmm15
0f 94 c0                  setz %al`],
     ['i64x2.all_true', `
33 c0                     xor %eax, %eax
66 45 0f ef ff            pxor %xmm15, %xmm15
66 44 0f 38 29 f8         pcmpeqq %xmm0, %xmm15
66 45 0f 38 17 ff         ptest %xmm15, %xmm15
0f 94 c0                  setz %al`]], {}
)

// Utils.
function codegenTestX64_v128_i32(inputs, options = {}) {
     for ( let [op, expected] of inputs ) {
         codegenTestX64_adhoc(wrap(options, `
     (func (export "f") (param v128) (result i32)
       (${op} (local.get 0)))`),
                              'f',
                              expected,
                              options);
     }
 }
