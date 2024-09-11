const tests = [
  [-0x7fffffffffffffffn, -0x8000000000000000n],
  [-0x7ffffffffffffffen, -0x7fffffffffffffffn],
  [-0x100000001n, -0x100000002n],
  [-0x100000000n, -0x100000001n],
  [-0xffffffffn, -0x100000000n],
  [-0xfffffffen, -0xffffffffn],
  [-0x80000001n, -0x80000002n],
  [-0x80000000n, -0x80000001n],
  [-0x7fffffffn, -0x80000000n],
  [-0x7ffffffen, -0x7fffffffn],
  [-2n, -3n],
  [-1n, -2n],
  [0n, -1n],
  [1n, 0n],
  [2n, 1n],
  [0x7ffffffen, 0x7ffffffdn],
  [0x7fffffffn, 0x7ffffffen],
  [0x80000000n, 0x7fffffffn],
  [0x80000001n, 0x80000000n],
  [0xfffffffen, 0xfffffffdn],
  [0xffffffffn, 0xfffffffen],
  [0x100000000n, 0xffffffffn],
  [0x100000001n, 0x100000000n],
  [0x7ffffffffffffffen, 0x7ffffffffffffffdn],
  [0x7fffffffffffffffn, 0x7ffffffffffffffen],
];

function f(tests) {
  for (let test of tests) {
    let input = test[0], expected = test[1];
    assertEq(BigInt.asIntN(64, input), input);
    assertEq(BigInt.asIntN(64, expected), expected);

    assertEq(--input, expected);
  }
}

for (let i = 0; i < 200; ++i) {
  f(tests);
}
