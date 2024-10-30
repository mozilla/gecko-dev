// |jit-test| -P wasm_lazy_tiering; -P wasm_lazy_tiering_synchronous; -P wasm_lazy_tiering_level=9;

let {test} = wasmEvalText(`(module
  (func $inlineMe (result i32)
    (local $i i32)
    loop
      block
        i32.const 0
        br_if 0

        local.get $i
        return
      end

      br 0
    end
    i32.const 2
  )
  (func (export "test") (result i32)
    call $inlineMe
  )
)`).exports;

for (let i = 0; i < 2; i++) {
  assertEq(test(), 0);
}
