// Test proper handling of OOM in masm.
function f() {
  new WebAssembly.Module(wasmTextToBinary("\
    (memory 64 64) (func (param i32 i32 i32)(local i32) loop block i32.const 0 \
    i32.extend8_s local.get 0 i32.const 0 i32.load align=1 return_call 0 end end) \
  "))
  oomTest(f);
}
f();
