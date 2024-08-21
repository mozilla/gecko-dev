const i64 = new BigInt64Array([
  0n,
  -0x8000_0000_0000_0000n,

  0n,
  -0x7fff_ffff_ffff_ffffn,

  0n,
  -0x8000_0000n,

  0n,
  -0x7fff_ffffn,

  0n,
  -1n,

  0n,
  1n,

  0n,
  0x7fff_ffffn,

  0n,
  0x7fff_ffff_ffff_ffffn,
]);

const u64 = new BigUint64Array([
  0n,
  1n,

  0n,
  0x7fff_ffffn,

  0n,
  0x8000_0000n,

  0n,
  0xffff_ffffn,

  0n,
  0x1_0000_0000n,

  0n,
  0x7fff_ffff_ffff_ffffn,

  0n,
  0x8000_0000_0000_0000n,

  0n,
  0xffff_ffff_ffff_ffffn,
]);

function testI64() {
  for (var i = 0; i < 200; ++i) {
    var v = i64[i & 15];
    if (v) {
      assertEq((i & 1), 1);
    } else {
      assertEq((i & 1), 0);
    }
  }
}
testI64();

function testU64() {
  for (var i = 0; i < 200; ++i) {
    var v = u64[i & 15];
    if (v) {
      assertEq((i & 1), 1);
    } else {
      assertEq((i & 1), 0);
    }
  }
}
testU64();
