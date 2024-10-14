// |jit-test| skip-if: !wasmGcEnabled(); test-also=-P wasm_lazy_tiering;

let {a} = wasmEvalText(`(module
  (type $t (func))
  (func (export "a")
    call $trap
    ref.null $t
    call_ref $t
  )
  (func $trap
    unreachable
  )
)`).exports;

const tierUpThreshold = 10;
for (let i = 0; i < tierUpThreshold; i++) {
  assertErrorMessage(a, WebAssembly.RuntimeError, /unreachable/);
}
