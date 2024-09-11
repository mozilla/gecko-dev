const tests = [
  [-0x80000000n, -0x7fffffffn],
  [-0x7fffffffn, -0x7ffffffen],
  [-0x7ffffffen, -0x7ffffffdn],
  [-2n, -1n],
  [-1n, 0n],
  [0n, 1n],
  [1n, 2n],
  [2n, 3n],
  [0x7ffffffen, 0x7fffffffn],
];

function f(tests) {
  for (let test of tests) {
    let input = test[0], expected = test[1];
    assertEq(BigInt.asIntN(32, input), input);
    assertEq(BigInt.asIntN(32, expected), expected);

    assertEq(++input, expected);
  }
}

for (let i = 0; i < 200; ++i) {
  f(tests);
}
