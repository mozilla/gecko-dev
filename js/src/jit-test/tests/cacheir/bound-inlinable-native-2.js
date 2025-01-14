// Test inlining bound function to inlinable natives when additional bound
// arguments are used.

function testMathMinBound1() {
  // Bind the inlinable native |Math.min|.
  // |Math.min| is inlined when up to four arguments are present.
  var MathMin = Math.min.bind(null, 4);

  for (var i = 0; i < 100; ++i) {
    assertEq(MathMin(i & 7), Math.min(i & 7, 4));
  }
}
testMathMinBound1();

function testMathMinBound2() {
  // Bind the inlinable native |Math.min|.
  // |Math.min| is inlined when up to four arguments are present.
  var MathMin = Math.min.bind(null, 4, 3);

  for (var i = 0; i < 100; ++i) {
    assertEq(MathMin(i & 7), Math.min(i & 7, 3));
  }
}
testMathMinBound2();

function testMathMinBound3() {
  // Bind the inlinable native |Math.min|.
  // |Math.min| is inlined when up to four arguments are present.
  var MathMin = Math.min.bind(null, 4, 3, 2);

  for (var i = 0; i < 100; ++i) {
    assertEq(MathMin(i & 7), Math.min(i & 7, 2));
  }
}
testMathMinBound3();

function testMathMinBound4() {
  // Bind the inlinable native |Math.min|.
  // |Math.min| is inlined when up to four arguments are present.
  var MathMin = Math.min.bind(null, 4, 3, 2, 1);

  for (var i = 0; i < 100; ++i) {
    assertEq(MathMin(), 1);
  }
}
testMathMinBound4();

function testMathMinBound4NoInline() {
  // Bind the inlinable native |Math.min|.
  // |Math.min| is inlined when up to four arguments are present.
  var MathMin = Math.min.bind(null, 4, 3, 2, 1);

  for (var i = 0; i < 100; ++i) {
    assertEq(MathMin(i & 7), Math.min(i & 7, 1));
  }
}
testMathMinBound4NoInline();

function testStringCharAt() {
  // Bind the inlinable native |String.prototype.charCodeAt| to
  // cover reading the |this| value.
  var str = "abcdefgh";
  var CharAt = String.prototype.charAt.bind(str, 0);

  for (var i = 0; i < 100; ++i) {
    assertEq(CharAt(), "a");
  }
}
testStringCharAt();

function testArrayConstructor() {
  // Bind the Array constructor function.
  var A = Array.bind(null, 10);

  for (var i = 0; i < 100; ++i) {
    var a = new A();
    assertEq(a.length, 10);
    assertEq(Object.getPrototypeOf(a), Array.prototype);
  }
}
testArrayConstructor();

function testMathMaxSpreadNoInline() {
  // Spread calls to bound functions aren't supported when additional bound
  // arguments are present.
  var MathMax = Math.max.bind(null, 0);

  for (var i = 0; i < 100; ++i) {
    var args = [i - 1, i, i + 1];
    assertEq(MathMax(...args), i + 1);
  }
}
testMathMaxSpreadNoInline();

function testMathMaxVariableBoundArgs() {
  // Bound Math.max with different number of bound arguments.
  var MathMax = [
    Math.max.bind(null, 4),
    Math.max.bind(null, 4, 5),
  ];

  for (var i = 0; i < 100; ++i) {
    assertEq(MathMax[i & 1](i & 7), Math.max(i & 7, 4 + (i & 1)));
  }
}
testMathMaxVariableBoundArgs();

function testFunctionBindWithBoundArgNoInline() {
  var array = [];

  // Bound function whose target is |Function.prototype.bind| and whose
  // this-value is |Array.prototype.push|. Also pass |array| as the this-value
  // for bound function which is returned when calling |FunBind()|.
  var FunBind = Function.prototype.bind.bind(Array.prototype.push, array);

  for (var i = 0; i < 100; ++i) {
    // Create a new bound function.
    //
    // This call to the bound target |Function.prototype.bind| won't be inlined,
    // because |FunBind| has additional bound arguments.
    var push = FunBind();

    assertEq(array.length, i * 3);
    push(1, 2, 3);
    assertEq(array.length, (i + 1) * 3);
  }
}
testFunctionBindWithBoundArgNoInline();
