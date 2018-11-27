// |jit-test| skip-if: !wasmGcEnabled() || !wasmDebuggingIsSupported()

(function() {
    let g = newGlobal();
    let dbg = new Debugger(g);
    g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (gc_feature_opt_in 2) (func (result anyref) (param anyref) get_local 0) (export "" 0))')));`);
})();

(function() {
    var g = newGlobal();
    g.parent = this;

    let src = `
      (module
        (gc_feature_opt_in 2)
        (func (export "func") (result anyref) (param $ref anyref)
            get_local $ref
        )
      )
    `;

    g.eval(`
        var obj = { somekey: 'somevalue' };
        Debugger(parent).onEnterFrame = function(frame) {
            let v = frame.environment.getVariable('var0');
            assertEq(typeof v, 'object');

            let prop = v.unwrap().getOwnPropertyDescriptor('somekey');
            assertEq(typeof prop, 'object');
            assertEq(typeof prop.value, 'string');
            assertEq(prop.value, 'somevalue');

            // Disable onEnterFrame hook.
            Debugger(parent).onEnterFrame = undefined;
        };
    `);

    new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`${src}`))).exports.func(g.obj);
})();
