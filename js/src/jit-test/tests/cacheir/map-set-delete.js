function test() {
    var m = new Map();
    var s = new Set();
    for (var i = 0; i < 100; i++) {
        assertEq(m.delete("a" + (i - 1)), i > 0);
        assertEq(m.delete("a" + (i - 1)), false);

        assertEq(s.delete("b" + (i - 1)), i > 0);
        assertEq(s.delete("b" + (i - 1)), false);

        m.set("a" + i, i);
        s.add("b" + i);
    }
    assertEq(m.size, 1);
    assertEq(s.size, 1);
}
test();
