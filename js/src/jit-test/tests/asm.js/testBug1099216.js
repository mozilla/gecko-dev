if (typeof SIMD === 'undefined' || !isSimdAvailable()) {
    print("won't run tests as simd extensions aren't activated yet");
    quit(0);
}

(function(global) {
    "use asm";
    var frd = global.Math.fround;
    var fx4 = global.SIMD.float32x4;
    var fsp = fx4.splat;
    function s(){}
    function d(x){x=fx4(x);}
    function e() {
        var x = frd(0);
        x = frd(x / x);
        s();
        d(fsp(x));
    }
    return e;
})(this)();

(function(m) {
    "use asm"
    var g = m.SIMD.int32x4
    var h = g.select
    function f() {
        var x = g(0, 0, 0, 0)
        var y = g(1, 2, 3, 4)
        return g(h(x, y, y))
    }
    return f;
})(this)();

t = (function(global) {
    "use asm"
    var toF = global.Math.fround
    var f4 = global.SIMD.float32x4
    function p(x, y, width, value, max_iterations) {
        x = x | 0
        y = y | 0
        width = width | 0
        value = value | 0
        max_iterations = max_iterations | 0
    }
    function m(xf, yf, yd, max_iterations) {
        xf = toF(xf)
        yf = toF(yf)
        yd = toF(yd)
        max_iterations = max_iterations | 0
        var _ = f4(0, 0, 0, 0), c_im4 = f4(0, 0, 0, 0)
        c_im4 = f4(yf, yd, yd, yf)
        return f4(c_im4);
    }
    return {p:p,m:m};
})(this)
t.p();
t.m();
