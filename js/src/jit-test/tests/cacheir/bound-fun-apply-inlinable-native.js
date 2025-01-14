// Test inlining bound fun_apply to inlinable natives.

function testNoBoundThis() {
  var ArrayJoin = Function.prototype.apply.bind(Array.prototype.join);

  var xs = [
    [],
    ["a"],
    ["a", "b"],
    ["a", "b", "c"],
  ];

  for (var i = 0; i < 100; ++i) {
    var x = xs[i & 3];
    assertEq(ArrayJoin(x), x.join());
    assertEq(ArrayJoin(x, null), x.join());
    assertEq(ArrayJoin(x, undefined), x.join());

    // Not optimized cases.
    assertEq(ArrayJoin(x, []), x.join());
    assertEq(ArrayJoin(x, [""]), x.join(""));
  }
}
testNoBoundThis();

function testBoundThis() {
  var array = ["a", "b"];
  var ArrayJoin = Function.prototype.apply.bind(Array.prototype.join, array);

  for (var i = 0; i < 100; ++i) {
    assertEq(ArrayJoin(), "a,b");
    assertEq(ArrayJoin(null), "a,b");
    assertEq(ArrayJoin(undefined), "a,b");

    // Not optimized cases.
    assertEq(ArrayJoin([]), "a,b");
    assertEq(ArrayJoin([""]), "ab");
  }
}
testBoundThis();

function testBoundThisAndArgs() {
  var array = ["a", "b"];
  var ArrayJoinNull = Function.prototype.apply.bind(Array.prototype.join, array, null);
  var ArrayJoinUndefined = Function.prototype.apply.bind(Array.prototype.join, array, undefined);
  var ArrayJoinEmptyArgs = Function.prototype.apply.bind(Array.prototype.join, array, []);
  var ArrayJoinWithArgs = Function.prototype.apply.bind(Array.prototype.join, array, [""]);

  for (var i = 0; i < 100; ++i) {
    assertEq(ArrayJoinNull(), "a,b");
    assertEq(ArrayJoinUndefined(), "a,b");

    // Not optimized cases.
    assertEq(ArrayJoinEmptyArgs(), "a,b");
    assertEq(ArrayJoinWithArgs(), "ab");
  }
}
testBoundThisAndArgs();

function testUndefinedGuard() {
  var array = ["a", "b"];

  var ArrayJoin = Function.prototype.apply.bind(Array.prototype.join);
  var ArrayJoinBoundThis = Function.prototype.apply.bind(Array.prototype.join, array);

  var args = [
    null,
    [""],
  ];
  var expected = [
    "a,b",
    "ab",
  ];

  for (var i = 0; i < 100; i++) {
    var index = (i > 50)|0;

    assertEq(ArrayJoin(array, args[index]), expected[index]);
    assertEq(ArrayJoinBoundThis(args[index]), expected[index]);
  }
}
testUndefinedGuard();

function testUndefinedBoundArgsGuard() {
  var array = ["a", "b"];

  var ArrayJoinBoundNull = Function.prototype.apply.bind(Array.prototype.join, array, null);
  var ArrayJoinBoundUndefined = Function.prototype.apply.bind(Array.prototype.join, array, undefined);
  var ArrayJoinBoundArgs = Function.prototype.apply.bind(Array.prototype.join, array, [""]);

  var fns = [
    [
      ArrayJoinBoundNull,
      ArrayJoinBoundArgs,
    ],
    [
      ArrayJoinBoundUndefined,
      ArrayJoinBoundArgs,
    ],
  ];
  var expected = [
    "a,b",
    "ab",
  ];

  for (var i = 0; i < 100; i++) {
    var index = (i > 50)|0;

    assertEq(fns[0][index](), expected[index]);
    assertEq(fns[1][index](), expected[index]);
  }
}
testUndefinedBoundArgsGuard();
