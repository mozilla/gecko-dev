// Test inlining bound fun_call to inlinable natives.

function testMathMin() {
  // Bind the inlinable native |Math.min|.
  var MathMin = Function.prototype.call.bind(Math.min);

  for (var i = 0; i < 100; ++i) {
    // Pass `undefined` as the unused |this|-value.
    assertEq(MathMin(undefined, i, 50), Math.min(i, 50));
  }
}
testMathMin();

function testMathMinMax() {
  // Bind the inlinable natives |Math.min| and |Math.max|.
  var MathMinMax = [
    Function.prototype.call.bind(Math.min),
    Function.prototype.call.bind(Math.max),
  ];

  // Compare against the non-bound |Math.min| and |Math.max|.
  var minmax = [
    Math.min,
    Math.max,
  ];

  for (var i = 0; i < 200; ++i) {
    // Pass `null` as the unused |this|-value.
    assertEq(MathMinMax[i & 1](null, i, 50), minmax[i & 1](i, 50));
  }
}
testMathMinMax();

function testMathMinBoundAndNonBound() {
  // Use bound and non-bound |Math.min|.
  var MathMin = [
    Function.prototype.call.bind(Math.min),
    Math.min,
  ];

  for (var i = 0; i < 200; ++i) {
    // Pass `Infinity` as the first argument. It's unused for the bound case and
    // never the result for the non-bound case.
    assertEq(MathMin[i & 1](Infinity, i, 50), Math.min(i, 50));
  }
}
testMathMinBoundAndNonBound();

function testStringCharCodeAt() {
  // Bind the inlinable native |String.prototype.charCodeAt| to
  // cover reading the |this| value.
  var str = "abcdefgh";
  var CharCodeAt = Function.prototype.call.bind(String.prototype.charCodeAt);

  for (var i = 0; i < 100; ++i) {
    assertEq(CharCodeAt(str, i & 7), str.charCodeAt(i & 7));
  }
}
testStringCharCodeAt();

function testStringCharCodeAtWithBoundArgs() {
  // We don't yet support additional bound arguments for bound fun_call.
  var str = "abcdefgh";
  var CharCodeAt = Function.prototype.call.bind(String.prototype.charCodeAt, str);

  for (var i = 0; i < 100; ++i) {
    assertEq(CharCodeAt(i & 7), str.charCodeAt(i & 7));
  }
}
testStringCharCodeAtWithBoundArgs();

function testMathRandomWithNoArgs() {
  // Bound fun_call called with no additional stack args.
  var MathRandom = Function.prototype.call.bind(Math.random);

  for (var i = 0; i < 100; ++i) {
    var r = MathRandom();
    assertEq(0 <= r && r < 1, true);
  }
}
testMathRandomWithNoArgs();
