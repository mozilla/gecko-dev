// Test inlining bound function to inlinable natives when no additional bound
// arguments are used.

function testMathMin() {
  // Bind the inlinable native |Math.min|.
  var MathMin = Math.min.bind();

  for (var i = 0; i < 100; ++i) {
    assertEq(MathMin(i, 50), Math.min(i, 50));
  }
}
testMathMin();

function testMathMinMax() {
  // Bind the inlinable natives |Math.min| and |Math.max|.
  var MathMinMax = [
    Math.min.bind(),
    Math.max.bind(),
  ];

  // Compare against the non-bound |Math.min| and |Math.max|.
  var minmax = [
    Math.min,
    Math.max,
  ];

  for (var i = 0; i < 200; ++i) {
    assertEq(MathMinMax[i & 1](i, 50), minmax[i & 1](i, 50));
  }
}
testMathMinMax();

function testMathMinBoundAndNonBound() {
  // Use bound and non-bound |Math.min|.
  var MathMin = [
    Math.min.bind(),
    Math.min,
  ];

  for (var i = 0; i < 200; ++i) {
    assertEq(MathMin[i & 1](i, 50), Math.min(i, 50));
  }
}
testMathMinBoundAndNonBound();

function testStringCharCodeAt() {
  // Bind the inlinable native |String.prototype.charCodeAt| to
  // cover reading the |this| value.
  var str = "abcdefgh";
  var CharCodeAt = String.prototype.charCodeAt.bind(str);

  for (var i = 0; i < 100; ++i) {
    assertEq(CharCodeAt(i & 7), str.charCodeAt(i & 7));
  }
}
testStringCharCodeAt();

function testArrayConstructor() {
  // Bind the Array constructor function.
  var A = Array.bind();

  for (var i = 0; i < 100; ++i) {
    var a = new A(i);
    assertEq(a.length, i);
    assertEq(Object.getPrototypeOf(a), Array.prototype);
  }
}
testArrayConstructor();

function testArrayConstructorSubclass() {
  // Bind the Array constructor function.
  var BoundArray = Array.bind();

  // Bound functions don't have a "prototype" property.
  assertEq("prototype" in BoundArray, false);

  // Add "prototype" so we can subclass from |BoundArray|.
  BoundArray.prototype = Array.prototype;
  
  // Class whose super-class is a bound function to the Array function.
  // This case isn't optimised, because the callee and |new.target| don't match. 
  class A extends BoundArray {}

  for (var i = 0; i < 100; ++i) {
    var a = new A(i);
    assertEq(a.length, i);
    assertEq(Object.getPrototypeOf(a), A.prototype);
  }
}
testArrayConstructorSubclass();

function testMathMaxSpread() {
  // Bind the inlinable native |Math.max|.
  var MathMax = Math.max.bind();

  for (var i = 0; i < 100; ++i) {
    var args = [i - 1, i, i + 1];
    assertEq(MathMax(...args), i + 1);
  }
}
testMathMaxSpread();

function testFunctionBind() {
  // Bound function whose target is |Function.prototype.bind| and whose
  // this-value is |Array.prototype.push|.
  var FunBind = Function.prototype.bind.bind(Array.prototype.push);

  for (var i = 0; i < 100; ++i) {
    var array = [];

    // Create a new bound function. This call is equivalent to calling
    // |Array.prototype.push.bind(array).
    //
    // This call to the bound target |Function.prototype.bind| can be inlined,
    // because |FunBind| has no additional bound arguments.
    var push = FunBind(array);

    assertEq(array.length, 0);
    push(1, 2, 3);
    assertEq(array.length, 3);
  }
}
testFunctionBind();
