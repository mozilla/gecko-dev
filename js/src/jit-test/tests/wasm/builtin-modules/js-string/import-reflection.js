// |jit-test| skip-if: !wasmJSStringBuiltinsEnabled();
let module = new WebAssembly.Module(wasmTextToBinary(`(module
  (func
    (import "wasm:js-string" "test")
    (param externref)
    (result i32)
  )
  (global (import "'" "string") (ref extern))
)`), {builtins: ['js-string'], importedStringConstants: "'"});
let imports = WebAssembly.Module.imports(module);

// All imports that refer to a builtin module are suppressed from import
// reflection.
assertEq(imports.length, 0);
