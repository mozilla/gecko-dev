function test() {
    var throwExc = false;
    var e = {m: {foreign() {
        if (throwExc) {
            throw new TypeError("hi");
        }
    }}};
    var bin = wasmTextToBinary(`
    (module
        (import "m" "foreign" (func $foreign))
        (func
            (export "f")
            try
                (call $foreign)
            end
        )
    )`);
    var mod = new WebAssembly.Module(bin);
    var inst = new WebAssembly.Instance(mod, e);
    for (var i = 0; i < 30; i++) {
        if (i === 20) {
            throwExc = true;
        }
        var ex = null;
        try {
            inst.exports.f();
        } catch (e) {
            ex = e;
        }
        if (i >= 20) {
            assertEq(ex.message, "hi");
        } else {
            assertEq(ex, null);
        }
    }
}
test();
