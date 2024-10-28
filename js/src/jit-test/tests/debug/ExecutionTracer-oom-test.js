
if (typeof enableExecutionTracing == "undefined") {
  quit();
}


var g = newGlobal({ newCompartment: true });
var dbg = new Debugger();
dbg.addDebuggee(g);

g.enableExecutionTracing();

oomTest(function () {
  g.eval(`[100].map(function f3(x) { return x; });`);
});

gc();

const trace = g.getExecutionTrace();

g.disableExecutionTracing();

assertEq(trace.length, 1);

const events = trace[0].events;
var err = events.find(e => e.kind == "Error");
// Guaranteeing an error is tricky. We don't have a ton of allocations inside
// the tracer to fail, and so with --ion we tend to get skipped over by
// oomTest's iterative failure logic. Accordingly, we just assert that if
// there is an error, it needs to be the last entry.
if (err) {
  assertEq(err, events[events.length - 1]);
}
