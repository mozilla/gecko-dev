const i64 = new BigInt64Array([
  0n,
  -0x8000_0000n,

  0n,
  -0x7fff_ffffn,

  0n,
  -2n,

  0n,
  2n,

  0n,
  0x7fff_ffffn,
]);

function testIPtr() {
  for (var i = 0; i < 200; ++i) {
    var v = i64[i % i64.length];

    // Apply an operation to execute BigInt as IntPtr codepaths.
    var x = v < 0 ? 1n : v > 0 ? -1n : 0n;
    v += x;

    if (v) {
      assertEq((i & 1), 1);
    } else {
      assertEq((i & 1), 0);
    }
  }
}
testIPtr();
