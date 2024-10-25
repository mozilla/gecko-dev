// Test if user code in interrupt handler causes the problem.

// Stay in wasm for long time (double loop) so timeout can be triggerred.
var ins = wasmEvalText(`(module
  (global $g (mut i32) (i32.const 0))
  
  (func $f0 (param i32) (result i64)
    (local $s i64)
    loop
      local.get $s
      global.get $g
      br_if 1

      i64.const 1
      i64.add
      local.set $s
      local.get 0
      i32.const 1
      i32.sub
      local.tee 0
      br_if 0
    end
    local.get $s
  )

  (func (export "f") (param i32)
    (local $i i32)
    local.get 0
    local.set $i
    loop
      global.get $g
      br_if 1

      local.get 0
      call $f0
      drop
      local.get $i
      i32.const 1
      i32.sub
      local.tee $i
      br_if 0
    end
  )

  (func (export "stop")
    i32.const 1
    global.set $g
  )
)`);

var promising = WebAssembly.promising(ins.exports.f);

timeout(0.1, function() {
  print("timeout!");
  ins.exports.stop();
  return true;
});

promising(200000);
