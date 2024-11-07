let { createArray } = wasmEvalText(`
(module
    (type $a (array f64))

    (func (export "createArray") (result anyref)
        (array.new $a f64.const 4 i32.const 1)
    )
)`).exports;

let a = createArray();

assertErrorMessage(() => {
    wasmGcReadField(a, 2);
}, WebAssembly.RuntimeError, /index out of bounds/);
