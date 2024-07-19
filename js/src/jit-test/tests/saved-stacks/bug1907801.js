let g = newGlobal({ newCompartment: true });
let dbg = Debugger(g);

function foo() {
  saveStack();
  dbg.getNewestFrame().eval("saveStack()");
}

let stack = saveStack();
dbg.onDebuggerStatement = bindToAsyncStack(foo, {stack: stack});
g.eval("debugger");
