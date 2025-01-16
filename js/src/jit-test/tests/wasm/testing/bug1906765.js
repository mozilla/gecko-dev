// |jit-test| skip-if: !('wasmDis' in this)

var x = wasmTextToBinary(`(module
    (func $yyy (import "" "f"))
    (export "f" (func $yyy))
  )`);
let importedFunc = new WebAssembly.Instance(new WebAssembly.Module(x), {
    "": {
    f: function () {},
    },
}).exports.f;

assertErrorMessage(() => wasmDis(importedFunc), Error, /function missing selected tier/);
