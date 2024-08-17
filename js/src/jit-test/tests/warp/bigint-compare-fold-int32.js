// Fold BigInt x Int32 comparison with constants.

let bigInts = [
  // Definitely heap digits.
  -(2n ** 1000n),

  // Definitely not heap digits.
  -1n,
  0n,
  1n,

  // Definitely heap digits.
  2n ** 1000n,
];

let int32s = [
  -0x8000_0000,
  -1,
  0,
  1,
  0x7fff_ffff,
];

function test() {
  for (var i = 0; i < 100; ++i) {
    let lhs = LHS;
    let rhs = RHS;

    assertEq(lhs === rhs, false);
    assertEq(rhs === lhs, false);

    assertEq(lhs !== rhs, true);
    assertEq(rhs !== lhs, true);

    assertEq(lhs == rhs, EQUALS);
    assertEq(rhs == lhs, EQUALS);

    assertEq(lhs != rhs, !EQUALS);
    assertEq(rhs != lhs, !EQUALS);

    assertEq(lhs < rhs, LESS_THAN);
    assertEq(rhs < lhs, !LESS_THAN && !EQUALS);

    assertEq(lhs <= rhs, LESS_THAN || EQUALS);
    assertEq(rhs <= lhs, !LESS_THAN || EQUALS);

    assertEq(lhs > rhs, !LESS_THAN && !EQUALS);
    assertEq(rhs > lhs, LESS_THAN);

    assertEq(lhs >= rhs, !LESS_THAN || EQUALS);
    assertEq(rhs >= lhs, LESS_THAN || EQUALS);
  }
}

for (let lhs of bigInts) {
  for (let rhs of int32s) {
    // Ensure Int32-typed.
    rhs |= 0;

    let equals = lhs == rhs;
    let lessThan = lhs < rhs;
    let fn = Function(
      `return ${test}`
      .replaceAll("LHS", `${lhs}n`)
      .replaceAll("RHS", `${rhs}`)
      .replaceAll("EQUALS", `${equals}`)
      .replaceAll("LESS_THAN", `${lessThan}`)
    )();
    fn();
  }
}
