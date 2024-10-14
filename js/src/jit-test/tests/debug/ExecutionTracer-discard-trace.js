// Test that collectNativeTrace discards the trace when setting nativeTracing
// to false

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
dbg.nativeTracing = true;

g.eval(`
function foo() {
}
foo();
`);

// This should discard the trace
dbg.nativeTracing = false;

dbg.nativeTracing = true;

g.eval(`
function bar() {
}
function foo() {
  bar();
}
foo();
`);

var trace = dbg.collectNativeTrace();

assertEq(dbg.nativeTracing, false);

// if the trace were not discarded earlier, we would see more than 4 entries
// here
assertEq(trace.events.length, 4);
assertEq(trace.events[0][0], Debugger.TRACING_EVENT_KIND_FUNCTION_ENTER);
assertEq(trace.events[1][0], Debugger.TRACING_EVENT_KIND_FUNCTION_ENTER);
assertEq(trace.events[2][0], Debugger.TRACING_EVENT_KIND_FUNCTION_LEAVE);
assertEq(trace.events[3][0], Debugger.TRACING_EVENT_KIND_FUNCTION_LEAVE);

assertEq(Object.keys(trace.atoms).length, 2);
assertEq(trace.atoms[trace.events[0][4]], "foo");
assertEq(trace.atoms[trace.events[1][4]], "bar");
assertEq(trace.events[2][4], trace.events[1][4]);
assertEq(trace.events[3][4], trace.events[0][4]);
