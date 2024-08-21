// Int32 values, including minimum, maximum, and values around zero.
const values = [
  [0x8000_0000|0, -0x80000000n],
  [0x8000_0001|0, -0x7fffffffn],
  [0x8000_0002|0, -0x7ffffffen],
  [0x8000_0003|0, -0x7ffffffdn],
  [-3, -3n],
  [-2, -2n],
  [-1, -1n],
  [0, 0n],
  [1, 1n],
  [2, 2n],
  [3, 3n],
  [0x7fff_fffd, 0x7fff_fffdn],
  [0x7fff_fffe, 0x7fff_fffen],
  [0x7fff_ffff, 0x7fff_ffffn],
];

const m = new WebAssembly.Module(wasmTextToBinary(`(module
  (func (export "toInt32") (param i64) (result i32)
    local.get 0
    i32.wrap_i64
  )
  (func (export "toInt64") (param i64) (result i64)
    local.get 0
  )
)`));

const {
  toInt32,
  toInt64,
} = new WebAssembly.Instance(m).exports;

function test() {
  for (let i = 0; i < 100; ++i) {
    assertEq(toInt32(BigInt(INT32)), INT32);
    assertEq(toInt64(BigInt(INT32)), INT64);
  }
}

for (let [int32, int64] of values) {
  let fn = Function(
    `return ${test}`
    .replaceAll("INT32", int32)
    .replaceAll("INT64", int64 + "n")
  )();
  fn();
}
