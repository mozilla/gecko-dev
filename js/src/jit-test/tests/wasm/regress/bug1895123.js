// |jit-test| slow

// Tests import stubs stack maps.

(function t() {
  timeout(.0001, () => {
    gc(); t(); return true;
  });
})();

var ins = wasmEvalText(`(module
  (import "" "f" (func $f (param externref i32) (param f64)))
  (export "main" (func $f))
)`, {"": {f: async (i) => { },},});

var t0 = performance.now();
while (performance.now() - t0 < 2000) {
  for (let i = 0; i < 100; i++)
     ins.exports.main(void 0, 1);
}
