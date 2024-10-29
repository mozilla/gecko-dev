// The execution tracing and the code coverage should be mutually exclusive.

if (typeof enableExecutionTracing == "undefined") {
  quit();
}

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger();
dbg.addDebuggee(g);

// Setting collectCoverageInfo should fail while the execution tracing is
// active, and it shouldn't affect the trace.

g.enableExecutionTracing();

g.eval(`[1].map(function f1(x) { return x; });`);

let caught = false;
try {
  dbg.collectCoverageInfo = true;
} catch (e) {
  caught = true;
  assertEq(e.message, "execution trace and collectCoverageInfo cannot be active at the same time");
}
assertEq(caught, true);

g.eval(`[10].map(function f2(x) { return x; });`);

caught = false;
try {
  dbg.collectCoverageInfo = false;
} catch (e) {
  caught = true;
  assertEq(e.message, "execution trace and collectCoverageInfo cannot be active at the same time");
}
assertEq(caught, true);

g.eval(`[100].map(function f3(x) { return x; });`);

const trace = g.getExecutionTrace();

g.disableExecutionTracing();

assertEq(trace.length, 1);

const events = trace[0].events;
assertEq(events.length, 6);

assertEq(events[0].kind, "FunctionEnter");
assertEq(events[0].lineNumber, 1);
assertEq(events[0].columnNumber, 20);
assertEq(events[0].script.includes("ExecutionTracer-coverage-exclusive.js"), true);
assertEq(events[0].script.endsWith(" > eval"), true);
assertEq(typeof events[0].realmID, "number");
assertEq(events[0].name, "f1");

assertEq(events[1].kind, "FunctionLeave");
assertEq(events[1].lineNumber, 1);
assertEq(events[1].columnNumber, 20);
assertEq(events[1].script.includes("ExecutionTracer-coverage-exclusive.js"), true);
assertEq(events[1].script.endsWith(" > eval"), true);
assertEq(typeof events[1].realmID, "number");
assertEq(events[1].name, "f1");

assertEq(events[2].kind, "FunctionEnter");
assertEq(events[2].lineNumber, 1);
assertEq(events[2].columnNumber, 21);
assertEq(events[2].script.includes("ExecutionTracer-coverage-exclusive.js"), true);
assertEq(events[2].script.endsWith(" > eval"), true);
assertEq(typeof events[2].realmID, "number");
assertEq(events[2].name, "f2");

assertEq(events[3].kind, "FunctionLeave");
assertEq(events[3].lineNumber, 1);
assertEq(events[3].columnNumber, 21);
assertEq(events[3].script.includes("ExecutionTracer-coverage-exclusive.js"), true);
assertEq(events[3].script.endsWith(" > eval"), true);
assertEq(typeof events[3].realmID, "number");
assertEq(events[3].name, "f2");

assertEq(events[4].kind, "FunctionEnter");
assertEq(events[4].lineNumber, 1);
assertEq(events[4].columnNumber, 22);
assertEq(events[4].script.includes("ExecutionTracer-coverage-exclusive.js"), true);
assertEq(events[4].script.endsWith(" > eval"), true);
assertEq(typeof events[4].realmID, "number");
assertEq(events[4].name, "f3");

assertEq(events[5].kind, "FunctionLeave");
assertEq(events[5].lineNumber, 1);
assertEq(events[5].columnNumber, 22);
assertEq(events[5].script.includes("ExecutionTracer-coverage-exclusive.js"), true);
assertEq(events[5].script.endsWith(" > eval"), true);
assertEq(typeof events[5].realmID, "number");
assertEq(events[5].name, "f3");

// Enabling/disabling the execution trace should fail while the code coverage
// is active, and it shouldn't affect the coverage result.

const scripts = [];
dbg.onNewScript = s => {
  scripts.push(s);
};

dbg.collectCoverageInfo = true;

g.eval(`[1000].map(function f4(x) { return x; });`);

caught = false;
try {
  g.enableExecutionTracing();
} catch (e) {
  caught = true;
  assertEq(e.message, "execution trace and collectCoverageInfo cannot be active at the same time");
}
assertEq(caught, true);

g.eval(`[10000].map(function f5(x) { return x; });`);

// This should be no-op.
g.disableExecutionTracing();

g.eval(`[100000].map(function f6(x) { return x; });`);

assertEq(scripts.length, 3);
for (const s of scripts) {
  const cov = s.getOffsetsCoverage();
  assertEq(typeof cov, "object");
}

dbg.collectCoverageInfo = false;

