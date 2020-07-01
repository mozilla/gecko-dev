// Test instrumenting generator creation. FIXME this doesn't actually test anything.

var g = newGlobal({ newCompartment: true });
var dbg = Debugger(g);
var gdbg = dbg.addDebuggee(g);

var allScripts = [];

function setScriptId(script) {
  script.setInstrumentationId(allScripts.length);
  allScripts.push(script);

  script.getChildScripts().forEach(setScriptId);
}

dbg.onNewScript = setScriptId;

const executedLines = [];
gdbg.setInstrumentation(
  gdbg.makeDebuggeeValue((kind, script, offset) => {
    print(`INSTRUMENT ${kind} ${script}`);
  }),
  ["generator", "entry"]
);

gdbg.setInstrumentationActive(true);

g.eval(`
dis(foo);
async function foo() {
  console.log("foo");
  bar();
  await baz(4);
}
function bar() {
  console.log("bar");
}
async function baz(n) {
  console.log("baz", n);
  if (n) {
    await new Promise(r => r());
    await baz(n - 1);
  }
}
`);

g.foo();
