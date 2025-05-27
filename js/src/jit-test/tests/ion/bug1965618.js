// |jit-test| --fast-warmup; --no-threads

let g = newGlobal({ newCompartment: true });
g.debuggee = this;
g.eval("Debugger(debuggee).onExceptionUnwind = function(){}");

function foo() {
  return dummy;
}

var arr = [1, -1,];
function bar(a) {
  let j = a % 3;
  if (arr[j]) {
    foo(arr[j] >>> 0, Math.ceil(~(arr[j] >>> 0)));
  }
}

with ({}) {}
for (var i = 0; i < 100; i++) {
  try { bar(i, i) } catch {}
}
