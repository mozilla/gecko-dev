// |jit-test| --fast-warmup; --no-threads; skip-if: !('stringRepresentation' in this)

// substring shouldn't create chains of dependent strings.

function test() {
  for (var i = 0; i < 100; i++) {
    var s = "abcdefabcdefabcdefabcdefabcdefabcdef";
    for (var j = 0; j < 4; j++) {
      s = s.substring(1);
    }
    var rep = JSON.parse(stringRepresentation(s));
    assertEq(rep.flags.includes("DEPENDENT_BIT"), true);
    assertEq(rep.base.flags.includes("DEPENDENT_BIT"), false);
    assertEq(s, "efabcdefabcdefabcdefabcdefabcdef");
  }
}
test();
