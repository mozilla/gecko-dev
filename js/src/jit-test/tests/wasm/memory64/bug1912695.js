const m = new WebAssembly.Module(wasmTextToBinary(`(module
  (memory i64 1 100 shared)
  (func (param i64)
    local.get 0
    i32.const 1
    memory.atomic.notify offset=29252962827
    drop
  )
)`));
