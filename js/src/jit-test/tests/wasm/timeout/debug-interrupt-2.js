// |jit-test| exitstatus: 6; skip-if: !wasmDebuggingIsSupported()

// Don't include wasm.js in timeout tests: when wasm isn't supported, it will
// quit(0) which will cause the test to fail.

var g = newGlobal();
g.parent = this;
g.eval("Debugger(parent).onEnterFrame = function() {};");
timeout(0.01);
var code = wasmTextToBinary(`(module
    (func $f2
        loop $top
            br $top
        end
    )
    (func (export "f1")
        call $f2
    )
)`);
new WebAssembly.Instance(new WebAssembly.Module(code)).exports.f1();
