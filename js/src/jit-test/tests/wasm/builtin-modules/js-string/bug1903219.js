// |jit-test| skip-if: !wasmJSStringBuiltinsEnabled();

// Check if we can expose builtin function.

var ins = wasmEvalText(`
(module
  (import "wasm:js-string" "charCodeAt"
    (func $charCodeAt (param externref i32) (result i32)))
  (table (export "t") 1 1 funcref ref.func $charCodeAt)
)
`, {}, { builtins: ["js-string"] });

assertEq(ins.exports.t.get(0)("abc", 1), 98);
