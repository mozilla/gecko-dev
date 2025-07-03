// Create a bunch of outstanding suspended stacks and test GC behaviour.

let promises = [];
function js_import() {
  return Promise.resolve(13);
}
let wasm_js_import = new WebAssembly.Suspending(js_import);

var ins = wasmEvalText(`(module
  (import "m" "import" (func (result externref)))
  (global (export "outstanding") (mut i32) (i32.const 0))
  (func (export "test") (param externref) (result externref)
    local.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0

    call 0
    drop

    global.get 0
    i32.const 1
    i32.sub
    global.set 0

    return
  )
)`, {
  m: {
    import: wasm_js_import
  },
});

let wrapped_export = WebAssembly.promising(ins.exports.test);

assertEq(ins.exports.outstanding.value, 0);

let count = 100;
for (let i = 0; i < count; i++) {
  wrapped_export({i}).then((x) => assertEq(x.i, i));
}

assertEq(ins.exports.outstanding.value, count);

gczeal(10, 2);
drainJobQueue();

assertEq(ins.exports.outstanding.value, 0);
