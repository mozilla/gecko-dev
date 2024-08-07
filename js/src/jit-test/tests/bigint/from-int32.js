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

for (let i = 0; i < 1000; ++i) {
  let vals = values[i % values.length];
  assertEq(BigInt(vals[0]), vals[1]);
}
