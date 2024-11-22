function test() {
    var m = new Map();
    var s = new Set();
    for (var i = 0; i < 100; i++) {
        m.set("a", 0).set(i, i).set("a", 1);
        s.add("a").add(i).add("a");
    }
    assertEq(m.size, 101);
    assertEq(s.size, 101);
}
test();
