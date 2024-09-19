// Test with suspended stack and promises references are lost.

let s;
const suspending = new WebAssembly.Suspending(() => (s = new Promise(() => {})));
const ins = wasmEvalText(`(module
  (import "" "s" (func $imp))
  (func (export "f")
    call $imp
  )
)`, {"": {s: suspending,}});
const promising = WebAssembly.promising(ins.exports.f);
let p = promising();

// Check if suspending and promising promises were collected.
addMarkObservers([s, p]);
s = p = null;
gc();
assertEq(getMarks()[0], 'dead');
assertEq(getMarks()[1], 'dead');
