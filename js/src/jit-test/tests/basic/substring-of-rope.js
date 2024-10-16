gczeal(0);

const hasStringRepresentation = typeof stringRepresentation !== "undefined";

function assertSameAddress(s1, s2) {
  if (hasStringRepresentation) {
    var s1Repr = stringRepresentation(s1);
    var s2Repr = stringRepresentation(s2);
    var s1Addr = JSON.parse(s1Repr).address;
    var s2Addr = JSON.parse(s2Repr).address;
    assertEq(typeof s1Addr, "string");
    assertEq(s1Addr, s2Addr);
  }
}

// rope.substring(0) should not allocate a new string or flatten the rope.
function testSameSubstring() {
  var rope = newRope("abcdefghijklmnop", "0123456789");
  for (var i = 0; i < 5; i++) {
    var sub = rope.substring(0);
    assertEq(isRope(sub), true);
    assertSameAddress(rope, sub);
  }
}
testSameSubstring();

// If the substring is exactly the left or right child, we should return it
// without flattening the rope.
function testLeftRightSubstring() {
  var left = "abcdefghijklmnopqrstuvwxyz";
  var right = "012345678901234567890123456789";
  var rope = newRope(left, right);
  for (var i = 0; i < 10; i++) {
    assertSameAddress(rope.substring(0, left.length), left);
    assertSameAddress(rope.substring(left.length), right);
    assertEq(isRope(rope), true); // Still a rope.
  }
}
testLeftRightSubstring();

function testSubstringSpansBoth() {
  var left = "abcdefghijklmnopqrstuvwxyz";
  var right = "012345678901234567890123456789";
  var rope = newRope(left, right);

  // If the substring spans both left and right children but fits in an inline
  // string, don't flatten the rope.
  for (var i = 0; i < 10; i++) {
    var substrInline = rope.substring(left.length - 5, left.length + 5);
    if (hasStringRepresentation) {
      assertEq(stringRepresentation(substrInline).includes("InlineString"), true);
    }
    assertEq(substrInline, "vwxyz01234");
    assertEq(isRope(rope), true); // Still a rope.
  }

  // If the substring doesn't fit in an inline string, we flatten the rope and
  // return a dependent string.
  var substrLong = rope.substring(0, rope.length - 1);
  assertEq(isRope(rope), false);
  assertEq(isRope(substrLong), false);
}
testSubstringSpansBoth();
