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
            (param i32)
            (result i32)
            (local i32)
            try
                call $foreign
                local.get 0
                local.set 1
            catch_all
                i32.const 12345
                local.get 0
                i32.add
                local.set 1
            end
            local.get 1
        )
    )`);
    var mod = new WebAssembly.Module(bin);
    var inst = new WebAssembly.Instance(mod, e);
    for (var i = 0; i < 30; i++) {
        if (i === 20) {
            throwExc = true;
        }
        var res = inst.exports.f(i);
        assertEq(res, throwExc ? (12345 + i) : i);
    }
}
test();
