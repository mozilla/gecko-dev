var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);

assertEq(dbg.nativeTracing, false);
dbg.nativeTracing = true;
assertEq(dbg.nativeTracing, true);
dbg.nativeTracing = false;
assertEq(dbg.nativeTracing, false);

assertEq(Debugger.TRACING_EVENT_KIND_FUNCTION_ENTER !== undefined, true);
assertEq(Debugger.TRACING_EVENT_KIND_FUNCTION_LEAVE !== undefined, true);
assertEq(Debugger.TRACING_EVENT_KIND_LABEL_ENTER !== undefined, true);
assertEq(Debugger.TRACING_EVENT_KIND_LABEL_LEAVE !== undefined, true);
assertEq(Debugger.IMPLEMENTATION_INTERPRETER !== undefined, true);
assertEq(Debugger.IMPLEMENTATION_BASELINE !== undefined, true);
assertEq(Debugger.IMPLEMENTATION_ION !== undefined, true);
assertEq(Debugger.IMPLEMENTATION_WASM !== undefined, true);
