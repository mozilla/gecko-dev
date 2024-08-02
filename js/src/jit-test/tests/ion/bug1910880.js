// |jit-test| --fast-warmup; skip-if: !wasmIsSupported()

gczeal(2);

function wasmEvalText(str) {
    var bin = wasmTextToBinary(str);
    var m = new WebAssembly.Module(bin);
    return new WebAssembly.Instance(m);
}
function test() {
    var instance = wasmEvalText(`
        (module (type $a (array (mut i32)))
        (func (export "createDefault") (param i32) (result eqref)
            local.get 0
            array.new_default $a
        )
        )
    `);
    var createDefault = instance.exports.createDefault;

    var g = newGlobal({newCompartment: true});
    g.debuggeeGlobal = this;
    g.eval("(" + function () {
        var dbg = new Debugger(debuggeeGlobal);
        dbg.onExceptionUnwind = function () {
            throw new Error("x");
        };
    } + ")();");

    for (var i = 0; i < 8; i++) {
        try {
            createDefault(-1);
        } catch (e) {
        }
    }
}
test();
quit(0); // Ensure exit code is 0, not 3.
