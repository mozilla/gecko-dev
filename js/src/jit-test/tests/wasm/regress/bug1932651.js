// |jit-test| skip-if: !wasmDebuggingEnabled()

newGlobal({sameZoneAs: this}).Debugger({});
const binary = wasmTextToBinary(`(module
  (func (export "") (result anyref i32)
    ref.null any
    ref.as_non_null
    ref.func 0
    ref.null any
    i32.const 0
    return
    i32.const 0
    i32.const 0
    i32.and
    unreachable
  )
)
`);
new WebAssembly.Module(binary);
