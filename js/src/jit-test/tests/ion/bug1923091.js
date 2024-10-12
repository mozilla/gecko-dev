// |jit-test| --fast-warmup

function testNot() {
  for (var i = 0; i < 100; i++) {
    assertEq(!BigInt(-1), false);
    assertEq(!BigInt(0), true);
    assertEq(!BigInt(1), false);
  }
}
testNot();

function testOr() {
  for (var i = 0; i < 100; i++) {
    var b0 = BigInt(0);
    var b5 = BigInt(5);
    assertEq(b5 || b0, b5);
    assertEq(b5 || 1, b5);
    assertEq(b0 || b5, b5);
    assertEq(1 || b5, 1);
  }
}
testOr();

function testAnd() {
  for (var i = 0; i < 100; i++) {
    var b0 = BigInt(0);
    var b1 = BigInt(1);
    assertEq(1 && b1, b1);
    assertEq(b0 && b1, b0);
    assertEq(b1 && b0, b0);
    assertEq(b1 && 1, 1);
  }
}
testAnd();
