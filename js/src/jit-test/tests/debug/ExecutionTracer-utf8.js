// Test that collectNativeTrace properly handles utf8 in script URLs

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var debuggee = dbg.getDebuggees()[0];
dbg.nativeTracing = true;

debuggee.executeInGlobal("(() => {})()", {url: "これはテストです"});

var trace = dbg.collectNativeTrace();

assertEq(trace.scriptURLs[trace.events[0][3]], "これはテストです");
