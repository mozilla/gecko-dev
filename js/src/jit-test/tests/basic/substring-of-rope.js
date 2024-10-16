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
