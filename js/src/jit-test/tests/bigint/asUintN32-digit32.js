// asUintN32 test specialised when the input and output are Int32 values.

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
];

function f(tests) {
  for (let test of tests) {
    let input = test[0], expected = test[1];

    assertEq(BigInt.asUintN(32, input), expected);
  }
}

for (let i = 0; i < 100; ++i) {
  f(tests);
}
