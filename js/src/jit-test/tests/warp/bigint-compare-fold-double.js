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

let doubles = [
  -Infinity,
  Number.MAX_VALUE,
  -Math.SQRT2,
  -Number.MIN_VALUE,
  -0,
  Number.MIN_VALUE,
  Math.SQRT2,
  Number.MAX_VALUE,
  Infinity,
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
  for (let rhs of doubles) {
    let equals = lhs == rhs;
    let lessThan = lhs < rhs;
    let repr = Object.is(rhs, -0) ? "-0" : String(rhs);
    let fn = Function(
      `return ${test}`
      .replaceAll("LHS", `${lhs}n`)
      .replaceAll("RHS", `${repr}`)
      .replaceAll("EQUALS", `${equals}`)
      .replaceAll("LESS_THAN", `${lessThan}`)
    )();
    fn();
  }
}

function testNaN() {
  for (var i = 0; i < 100; ++i) {
    let lhs = LHS;
    let rhs = NaN;

    assertEq(lhs === rhs, false);
    assertEq(rhs === lhs, false);

    assertEq(lhs !== rhs, true);
    assertEq(rhs !== lhs, true);

    assertEq(lhs == rhs, false);
    assertEq(rhs == lhs, false);

    assertEq(lhs != rhs, true);
    assertEq(rhs != lhs, true);

    assertEq(lhs < rhs, false);
    assertEq(rhs < lhs, false);

    assertEq(lhs <= rhs, false);
    assertEq(rhs <= lhs, false);

    assertEq(lhs > rhs, false);
    assertEq(rhs > lhs, false);

    assertEq(lhs >= rhs, false);
    assertEq(rhs >= lhs, false);
  }
}

for (let lhs of bigInts) {
  let fn = Function(
    `return ${testNaN}`
    .replaceAll("LHS", `${lhs}n`)
  )();
  fn();
}
