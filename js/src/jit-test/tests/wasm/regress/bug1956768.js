let testModule = `(module
(type $arrayMutI16 (array (mut i16)))

(func $fromCharCodeArray
    (import "wasm:js-string" "fromCharCodeArray")
    (param (ref null $arrayMutI16) i32 i32)
    (result (ref extern)))

(func (export "test") (result externref)
  (array.new_fixed $arrayMutI16 4 (i32.const 0) (i32.const 1) (i32.const 2) (i32.const 3))
  i32.const 0
  i32.const 4
  call $fromCharCodeArray
)
)`;
let module = new WebAssembly.Module(wasmTextToBinary(testModule), {builtins: ['js-string']});
let instance = new WebAssembly.Instance(module, {});

oomTest(() => {
    let result = instance.exports.test();
    assertEq(result, "\x00\x01\x02\x03");
});
