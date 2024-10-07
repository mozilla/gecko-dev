function f() {
    var g = function() { return 1; };
    for (var i = 0; i < 20; i++) {
        g();
    }
    try {
        disnative(g);
    } catch {}
}
f();
