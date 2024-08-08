const values = [
  -Infinity,
  -Number.MAX_VALUE,
  -Number.MIN_VALUE,
  -0,

  +0,
  +Number.MIN_VALUE,
  +Number.MAX_VALUE
  +Infinity,

  -123,
  -0.5,
  +123,
  +0.5,
];

for (let i = 0; i < 200; ++i) {
  let val = values[i % values.length];

  // rhs is NaN
  assertEq(val + NaN, NaN);
  assertEq(val - NaN, NaN);
  assertEq(val * NaN, NaN);
  assertEq(val / NaN, NaN);
  assertEq(val % NaN, NaN);
  assertEq(val ** NaN, NaN);

  // rhs is undefined, ToNumber(undefined) is NaN
  assertEq(val + undefined, NaN);
  assertEq(val - undefined, NaN);
  assertEq(val * undefined, NaN);
  assertEq(val / undefined, NaN);
  assertEq(val % undefined, NaN);
  assertEq(val ** undefined, NaN);

  // lhs is NaN
  assertEq(NaN + val, NaN);
  assertEq(NaN - val, NaN);
  assertEq(NaN * val, NaN);
  assertEq(NaN / val, NaN);
  assertEq(NaN % val, NaN);
  assertEq(NaN ** val, (val === 0 ? 1 : NaN));

  // lhs is undefined, ToNumber(undefined) is NaN
  assertEq(undefined + val, NaN);
  assertEq(undefined - val, NaN);
  assertEq(undefined * val, NaN);
  assertEq(undefined / val, NaN);
  assertEq(undefined % val, NaN);
  assertEq(undefined ** val, (val === 0 ? 1 : NaN));
}
