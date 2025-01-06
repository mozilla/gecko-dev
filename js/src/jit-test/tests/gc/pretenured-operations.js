// Check individual operations that support pretenuring their result.

load(libdir + "pretenure.js");

let a;

function run(op) {
  a = [];
  for (let i = 0; i < 1000; i++) {
    a.push(op());
  }
  return a[0];
}

function check(op) {
  assertEq(isNurseryAllocated(run(op)), true);
  minorgc();
  assertEq(isNurseryAllocated(run(op)), false);

  a = undefined;
  gc();
}

setupPretenureTest();

// Object allocation.
check(() => { return {}; });

// Array allocation.
check(() => { return []; });

// Object construction.
check(() => { return new Object(); });
check(() => { return Object(); });

// Array construction.
check(() => { return Array(); });
check(() => { return Array(150); });
check(() => { return new Array(); });
check(() => { return new Array(150); });

// DOM Allocations
let fdo = new FakeDOMObject();
check(() => { return fdo.doBar(); })
