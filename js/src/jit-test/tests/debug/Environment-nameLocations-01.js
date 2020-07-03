// FIXME update this test to actually test something.

var g = newGlobal({ newCompartment: true });
var dbg = Debugger(g);
var gdbg = dbg.addDebuggee(g);

g.eval(`
function z() {
  let a = 0;
  var b = 1;
  function foo(z, { d }) {
    let x = 0, y = 1, { d2 } = d;
    var w = 2;
    debugger;
  }
  foo(0, { d: { d2: 0 } });
}
`);

dbg.onDebuggerStatement = frame => {
  let env = frame.environment;
  while (env) {
    print("ENV " + env.scopeKind + " " + JSON.stringify(env.nameLocations()));
    env = env.parent;
  }
}

g.z();
