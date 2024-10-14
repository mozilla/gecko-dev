// Test that collectNativeTrace returns a well formed execution trace

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
dbg.nativeTracing = true;

var start = performance.now();

g.eval(`
function bar() {
}
function foo() {
  bar();
}
foo();
`);

var end = performance.now();

var trace = dbg.collectNativeTrace();

assertEq(dbg.nativeTracing, false);

assertEq(trace.events[0][0], Debugger.TRACING_EVENT_KIND_FUNCTION_ENTER);
assertEq(trace.events[1][0], Debugger.TRACING_EVENT_KIND_FUNCTION_ENTER);
assertEq(trace.events[2][0], Debugger.TRACING_EVENT_KIND_FUNCTION_LEAVE);
assertEq(trace.events[3][0], Debugger.TRACING_EVENT_KIND_FUNCTION_LEAVE);

assertEq(trace.events[0][1], 4); // foo line number
assertEq(trace.events[1][1], 2); // bar line number

assertEq(trace.events[0][2], 13); // foo column
assertEq(trace.events[1][2], 13); // bar column

assertEq(trace.atoms[trace.events[0][4]], "foo");
assertEq(trace.atoms[trace.events[1][4]], "bar");
assertEq(trace.events[2][4], trace.events[1][4]);
assertEq(trace.events[3][4], trace.events[0][4]);

// Ion should be disabled as an implementation, but if we're jit testing,
// we can hit baseline here
assertEq([Debugger.IMPLEMENTATION_INTERPRETER,
          Debugger.IMPLEMENTATION_BASELINE].includes(trace.events[0][5]), true);
assertEq([Debugger.IMPLEMENTATION_INTERPRETER,
          Debugger.IMPLEMENTATION_BASELINE].includes(trace.events[1][5]), true);

assertEq(trace.events[0][6] >= start, true);
assertEq(trace.events[0][6] <= end, true);
