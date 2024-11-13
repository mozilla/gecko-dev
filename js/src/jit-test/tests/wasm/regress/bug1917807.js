// |jit-test| --wasm-compiler=ion; --blinterp-eager

// Checks JIT exit parameters alignment.

function processWAST(source) {
    let modBuf = wasmTextToBinary(source);
    let module = new WebAssembly.Module(modBuf);
    let instance = new WebAssembly.Instance(module);
    for (let i = 0; i < 10; ++i)  {
        try {
            instance.exports.odd();
        } catch (e) {}
    }
}

processWAST(`(module
(table 2 2 funcref)
(elem (i32.const 0) $odd $odd)
(type $t (func (param) (result i32)))
(func $odd (export "odd") (param i32 i32 i32 i32 i32 i32 i32) (result i32)
  (return_call_indirect (type $t) (i32.const 1) (local.get 1))
))`);
