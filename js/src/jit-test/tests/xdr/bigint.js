// |jit-test| skip-if: !('BigInt' in this)
load(libdir + 'bytecode-cache.js');

let test = `
  assertEq(2n**64n - 1n, BigInt("0xffffFFFFffffFFFF"));

  // BigInt constants near INT64_MAX
  assertEq(0x7fff_ffff_ffff_ffffn + 1n, BigInt("0x8000000000000000"));
  assertEq(0x8000_0000_0000_0000n + 2n, BigInt("0x8000000000000002"));

  // BigInt constants near INT64_MIN
  assertEq(-0x7fff_ffff_ffff_ffffn - 1n, -BigInt("0x8000000000000000"));
  assertEq(-0x8000_0000_0000_0000n - 2n, -BigInt("0x8000000000000002"));
  assertEq(-0x8000_0000_0000_0001n - 3n, -BigInt("0x8000000000000004"));

  // BigInt constants near UINT64_MAX
  assertEq(0xffff_ffff_ffff_ffffn + 1n, BigInt("0x10000000000000000"));
  assertEq(0x1_0000_0000_0000_0000n + 2n, BigInt("0x10000000000000002"));
`;
evalWithCache(test, {
  assertEqBytecode: true,
  assertEqResult : true
});
