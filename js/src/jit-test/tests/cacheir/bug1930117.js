// |jit-test| --fast-warmup; --inlining-entry-threshold=10
var x = {};
function f(y, z) {
    if (Object.hasOwn(x, y)) {
        return;
    }
    var m;
    if (z === 1) {
        m = {n: [0]};
    } else if (z === 2) {
        m = {};
        m.n = [0];
    } else {
        Object.defineProperty(x, 0, {a: 1});
        return;
    }
    Object.defineProperty(x, y, {});
    assertEq(m.n[0], 0);
}
for (var i = 0; i < 7; i++) {
    f("a", 0);
}
f("b", 1);
f("1", 2);
f("a", 0);
f("2", 1);
f("2", 1);
f("2", 1);
oomTest(function () { Object.defineProperty(); });
