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

function testSimple() {
  for (let i = 0; i < 1000; ++i) {
    let vals = values[i % values.length];

    assertEq(toInt32(BigInt(vals[0])), vals[0]);
    assertEq(toInt64(BigInt(vals[0])), vals[1]);
  }
}
testSimple();

function testBoxed() {
  for (let i = 0; i < 1000; ++i) {
    let vals = values[i % values.length];

    // NB: The parser can only fold `if (true) { ... }`. Using an explicit
    // variable ensures that only GVN will fold the test-condition.
    const True = true;

    // Conditionally set |bi| to create a phi-value. MToInt64's type policy will
    // see a boxed input and therefore not apply MTruncateBigIntToInt64.
    //
    // Later let GVN simplify MToInt64(MInt32ToBigInt(i32)) to just
    // MExtendInt32ToInt64(i32).
    //
    // MBox(MInt32ToBigInt(i32)) is captured by a resume point, so we can't
    // recover MInt32ToBigInt(i32), because MBox is not recoverable.
    let bi = undefined;
    if (True) {
      bi = BigInt(vals[0]);
    }

    assertEq(toInt32(bi), vals[0]);
    assertEq(toInt64(bi), vals[1]);
  }
}
testBoxed();

function testRecover() {
  for (let i = 0; i < 1000; ++i) {
    let vals = values[i % values.length];

    // NB: The parser can only fold `if (true) { ... }`. Using an explicit
    // variable ensures that only GVN will fold the test-condition.
    const True = true;

    // Conditionally set |bi| to create a phi-value. MToInt64's type policy will
    // see a BigInt input and therefore apply MTruncateBigIntToInt64.
    //
    // Later let GVN simplify MTruncateBigIntToInt64(MInt32ToBigInt(i32)) to
    // just MExtendInt32ToInt64(i32).
    //
    // MInt32ToBigInt(i32) is captured by a resume point, but since
    // MInt32ToBigInt is recoverable, it will be optimized away.
    let bi = 0n;
    if (True) {
      bi = BigInt(vals[0]);
    }

    assertEq(toInt32(bi), vals[0]);
    assertEq(toInt64(bi), vals[1]);
  }
}
testRecover();
