// Use different types to ensure we compile to MSameValue.
var xs = [
  null,
  undefined,
  {},
  123,
  NaN,
  false,
  Symbol(),
  "",
];

// Object.is(input, null) is folded to |input === null|.
function testNull() {
  for (var i = 0; i < 500; ++i) {
    var x = xs[i & 7];
    assertEq(Object.is(null, x), x === null);
    assertEq(Object.is(x, null), x === null);
  }
}
for (let i = 0; i < 2; ++i) testNull();

// Object.is(input, undefined) is folded to |input === undefined|.
function testUndefined() {
  for (var i = 0; i < 500; ++i) {
    var x = xs[i & 7];
    assertEq(Object.is(undefined, x), x === undefined);
    assertEq(Object.is(x, undefined), x === undefined);
  }
}
for (let i = 0; i < 2; ++i) testUndefined();
