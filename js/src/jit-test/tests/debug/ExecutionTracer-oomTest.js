var g = newGlobal({newCompartment: true});
var dbg = new Debugger();

function test() {
    dbg.nativeTracing = true;
    g.eval(`
        function bar() {
        }
        function foo() {
          bar();
        }
        foo();
    `);

    dbg.collectNativeTrace();
}

oomTest(() => { test() });
