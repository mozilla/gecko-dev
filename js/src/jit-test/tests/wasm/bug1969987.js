
// This exposed an obscure bug to do with flag handling in GVN.

const t =`
(module
  (func (export "w0")
    (local $v1 i32)
    (local $v4 i32)
    (local $v5 i32)
    (local $v7 i32)
    (local $v8 i32)
    (local $v10 i32)
    i32.const 1
    local.set $v1

    local.get $v4
    local.get $v1
    local.get $v1
    local.get $v1

    loop (param i32 i32 i32 i32) ;; label = @1
      local.set $v5
      local.set $v7
      local.set $v8
      local.set $v10

      local.get $v8
      local.get $v4
      local.get $v8
      local.get $v7

      local.get $v10
      br_if 0 (;@1;)

      drop
      drop
      drop
      drop

      local.get $v1
      local.get $v4
      local.get $v10
      local.get $v1

      local.get $v5
      br_if 0 (;@1;)

      drop
      drop
      drop
      drop

    end
  )
)`;

// Check we can compile and run this without asserting.
const v52
      = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(t)));
v52.exports.w0();
