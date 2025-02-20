let module = new WebAssembly.Module(wasmTextToBinary(`(module
  (func (export "rshift") (param $x i64) (result i64)
    local.get $x
    local.get $x
    i64.shr_s
))`));

let instance = new WebAssembly.Instance(module);

let {rshift} = instance.exports;

for (let i = 0; i < 100; ++i) {
  // Input which uses both int64 register pairs on 32-bit systems.
  let x = BigInt(i) + 0x1_0000_0000n;

  assertEq(rshift(x), BigInt.asIntN(64, x >> (x & 63n)));
}
