// |jit-test| skip-if: !wasmIsSupported()

const bytes = wasmTextToBinary(`(module
  (table 0 externref)
  (func
    block
      unreachable
      table.fill 0
  )
)`);
assertEq(WebAssembly.validate(bytes), false);
assertErrorMessage(() => new WebAssembly.Module(bytes), WebAssembly.CompileError, /unable to read opcode/);
