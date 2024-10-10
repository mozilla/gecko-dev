// Multiple promises at the same time.

function js_import() {
    return Promise.resolve(42);
}
var wasm_js_import = new WebAssembly.Suspending(js_import);

var ins = wasmEvalText(`(module
    (import "m" "import" (func $f (result i32)))
    (func (export "test") (result i32)
      call $f
    )
)`, {"m": {import: wasm_js_import}});

let wrapped_export = WebAssembly.promising(ins.exports.test);

Promise.resolve().then(() => {
    wrapped_export().then(i => {
        assertEq(42, i)
    });
});

Promise.resolve().then(() => {
    wrapped_export().then(i => {
        assertEq(42, i)
    });
});
