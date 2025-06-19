// |jit-test| skip-if: !('toResizableBuffer' in WebAssembly.Memory.prototype)

// Check if OOM in toFixedLengthBuffer is handled properly.

function f() {
  var x = { initial: 0, maximum: 1 };
  x.shared = {};
  y = new WebAssembly.Memory(x);
  y.toFixedLengthBuffer(y.grow(x.maximum));
}
oomTest(() => {
  for (let i = 0; i < 4; i++) f();
});
