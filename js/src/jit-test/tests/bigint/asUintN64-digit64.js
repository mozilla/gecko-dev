// asUintN64 test specialised when the input and output are Int64 values.

const tests = [
  [0n, 0n],
  [1n, 1n],
  [2n, 2n],
  [3n, 3n],
  [4n, 4n],
  [5n, 5n],
  [6n, 6n],
  [7n, 7n],
  [8n, 8n],
  [9n, 9n],
  [0x7ffffffen, 0x7ffffffen],
  [0x7fffffffn, 0x7fffffffn],
  [0x80000000n, 0x80000000n],
  [0x80000001n, 0x80000001n],
  [0xfffffffen, 0xfffffffen],
  [0xffffffffn, 0xffffffffn],
  [0x100000000n, 0x100000000n],
  [0x100000001n, 0x100000001n],
  [0x7ffffffffffffffen, 0x7ffffffffffffffen],
  [0x7fffffffffffffffn, 0x7fffffffffffffffn],
];

function f(tests) {
  for (let test of tests) {
    let input = test[0], expected = test[1];

    assertEq(BigInt.asUintN(64, input), expected);
  }
}

for (let i = 0; i < 100; ++i) {
  f(tests);
}
