// Test shutting down the program with suspended stack.

const suspending = new WebAssembly.Suspending(() => new Promise(() => {}));
const ins = wasmEvalText(`(module
  (import "" "s" (func $imp))
  (func (export "f")
    call $imp
  )
)`, {"": {s: suspending,}});
const promising = WebAssembly.promising(ins.exports.f);
var p = promising();
