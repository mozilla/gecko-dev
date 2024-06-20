// |jit-test| --fast-warmup; --no-threads; skip-if: !('disassemble' in this)
Object.defineProperty(RegExp.prototype, "flags", {get: function() {
    Array.prototype.push.call(this);
}});
function f() {
    var s = disassemble();
    for (var i = 0; i < 30; i++) {
        s.replace(/0./);
    }
}
f();
