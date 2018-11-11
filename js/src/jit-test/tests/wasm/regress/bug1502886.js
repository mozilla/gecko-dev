newGlobal();
g = newGlobal();
var dbg = Debugger(g);
gczeal(2, 100);
function f(x, initFunc) {
    newGlobal();
    g.eval(`
        var {
            binary,
            offsets
        } = wasmTextToBinary('${x}', true);
        new WebAssembly.Instance(new WebAssembly.Module(binary));
    `);
    var {
        offsets
    } = g;
    var wasmScript = dbg.findScripts().filter(s => s.format == 'wasm')[0];
    initFunc({
        wasmScript,
        breakpoints: offsets
    })
};
try {
    f('(module (funcnopnop)(export "" 0))',
        function({
            wasmScript,
            breakpoints
        }) {
            breakpoints.forEach(function(offset) {
                wasmScript.setBreakpoint(offset, s = {});
            });
        }
    );
    f();
} catch (e) {}
