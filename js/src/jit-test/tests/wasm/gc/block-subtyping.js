// Subtyping should be used when an if-else has no else.
wasmEvalText(`(module
  (func (param structref) (result anyref)
    local.get 0
    i32.const 0
    if (param structref) (result anyref) end
  )
)`);
