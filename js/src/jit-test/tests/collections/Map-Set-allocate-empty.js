// Test (JIT) allocation of empty Map/Set objects and operations on them.

function testMap() {
    for (var i = 0; i < 100; i++) {
        var m = new Map();
        assertEq(m.size, 0);
        assertEq(m.has(this), false);
        assertEq(m.get(this), undefined);
        assertEq(m.delete(this), false);
        m.clear();
        var it = m.values();
        assertEq(it.next().done, true);
    }
}
testMap();

function testSet() {
    for (var i = 0; i < 100; i++) {
        var s = new Set();
        assertEq(s.size, 0);
        assertEq(s.has(this), false);
        assertEq(s.delete(this), false);
        s.clear();
        var it = s.values();
        assertEq(it.next().done, true);
    }
}
testSet();
