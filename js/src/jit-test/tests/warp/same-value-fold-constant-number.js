function SameValue(x, y) {
    if (x === y) {
        return (x !== 0) || (1 / x === 1 / y);
    }
    return (x !== x && y !== y);
}

var xs = [
  NaN,
  +0,
  -0,
  1,
  1.5,
  -1.5,
  Infinity,
  -Infinity,
];

// Object.is(input, NaN) is folded to |input !== input|.
function testNaN() {
  for (var i = 0; i < 500; ++i) {
    var x = xs[i & 7];
    assertEq(typeof x, "number");
    assertEq(Object.is(NaN, x), x !== x);
    assertEq(Object.is(x, NaN), x !== x);
  }
}
for (let i = 0; i < 2; ++i) testNaN();

// Object.is(input, +0) is folded to a bitwise comparison.
function testPositiveZero() {
  for (var i = 0; i < 500; ++i) {
    var x = xs[i & 7];
    assertEq(typeof x, "number");
    assertEq(Object.is(+0, x), SameValue(x, +0));
    assertEq(Object.is(x, +0), SameValue(x, +0));
  }
}
for (let i = 0; i < 2; ++i) testPositiveZero();

// Object.is(input, -0) is folded to a bitwise comparison.
function testNegativeZero() {
  for (var i = 0; i < 500; ++i) {
    var x = xs[i & 7];
    assertEq(typeof x, "number");
    assertEq(Object.is(-0, x), SameValue(x, -0));
    assertEq(Object.is(x, -0), SameValue(x, -0));
  }
}
for (let i = 0; i < 2; ++i) testNegativeZero();

// Object.is(input, int32) is folded to |input === int32|, when int32 isn't 0.
function testInt32() {
  for (var i = 0; i < 500; ++i) {
    var x = xs[i & 7];
    assertEq(typeof x, "number");
    assertEq(Object.is(1, x), x === 1);
    assertEq(Object.is(x, 1), x === 1);
  }
}
for (let i = 0; i < 2; ++i) testInt32();

// Object.is(input, double) is folded to |input === double|, when double is
// neither NaN, nor +/-0.
function testDouble() {
  for (var i = 0; i < 500; ++i) {
    var x = xs[i & 7];
    assertEq(typeof x, "number");
    assertEq(Object.is(1.5, x), x === 1.5);
    assertEq(Object.is(x, 1.5), x === 1.5);
  }
}
for (let i = 0; i < 2; ++i) testDouble();
