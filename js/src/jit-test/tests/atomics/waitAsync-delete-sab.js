// |jit-test| --setpref=atomics_wait_async=true; skip-if: helperThreadCount() === 0;

// Test deleting the SAB and timing out.
function test1() {
  var o = {};
  o.sab = new SharedArrayBuffer(4096);
  o.ia = new Int32Array(o.sab);
  o.ia[37] = 0x1337;

  var promise = Atomics.waitAsync(o.ia, 37, 0x1337, 1).value;
  assertEq(!!o.sab, true);
  delete o.sab;
  assertEq(!!o.sab, false);
  return promise;
}
// Test deleting the SAB and notify.
function test2() {
  var o = {};
  o.sab = new SharedArrayBuffer(4096);
  o.ia = new Int32Array(o.sab);
  o.ia[37] = 0x1337;

  var promise = Atomics.waitAsync(o.ia, 37, 0x1337, 100).value;
  assertEq(!!o.sab, true);
  delete o.sab;
  assertEq(!!o.sab, false);
  Atomics.notify(o.ia, 37);
  return promise;
}

// Test deleting the SAB without timeout and notify.
function test3() {
  var o = {};
  o.sab = new SharedArrayBuffer(4096);
  o.ia = new Int32Array(o.sab);
  o.ia[37] = 0x1337;

  var promise = Atomics.waitAsync(o.ia, 37, 0x1337).value;
  assertEq(!!o.sab, true);
  delete o.sab;
  assertEq(!!o.sab, false);
  Atomics.notify(o.ia, 37);
  return promise;
}

// Test deleting the SAB with immediate resolution.
function test4() {
  var o = {};
  o.sab = new SharedArrayBuffer(4096);
  o.ia = new Int32Array(o.sab);
  o.ia[37] = 0x1337;

  var v = Atomics.waitAsync(o.ia, 37, 0x1337, 0).value;
  assertEq(!!o.sab, true);
  delete o.sab;
  assertEq(!!o.sab, false);
  assertEq(v, "timed-out");
  return v;
}

// Store the result of the tests when run in the worker thread.
// Each test increments this value by one if it succeeds
let sab = new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT);
setSharedObject(sab);

// same tests as above but with a worker
function test5() {
	evalInWorker(`
  const i32 = new Int32Array(getSharedObject());
  function test1() {
    var o = {};
    o.sab = new SharedArrayBuffer(4096);
    o.ia = new Int32Array(o.sab);
    o.ia[37] = 0x1337;

    var promise = Atomics.waitAsync(o.ia, 37, 0x1337, 1).value;
    delete o.sab;
    return promise;
  }
  // Test deleting the SAB and notify.
  function test2() {
    var o = {};
    o.sab = new SharedArrayBuffer(4096);
    o.ia = new Int32Array(o.sab);
    o.ia[37] = 0x1337;

    var promise = Atomics.waitAsync(o.ia, 37, 0x1337, 10).value;
    delete o.sab;
    Atomics.notify(o.ia, 37);
    return promise;
  }
  // Test deleting the SAB without timeout and notify.
  function test3() {
    var o = {};
    o.sab = new SharedArrayBuffer(4096);
    o.ia = new Int32Array(o.sab);
    o.ia[37] = 0x1337;

    var promise = Atomics.waitAsync(o.ia, 37, 0x1337).value;
    delete o.sab;
    Atomics.notify(o.ia, 37);
    return promise;
  }

  // Test deleting the SAB with immediate resolution.
  function test4() {
    var o = {};
    o.sab = new SharedArrayBuffer(4096);
    o.ia = new Int32Array(o.sab);
    o.ia[37] = 0x1337;

    var v = Atomics.waitAsync(o.ia, 37, 0x1337, 0).value;
    delete o.sab;
    return v;
  }

  // Custom Timeout to ensure that we wait until waitAsync times out.
  function timeout(n) {
    var start = Date.now();
    while (Date.now() - start < n) {};
  }

  var result = "";
  test1()
    .then((v) => { result = v });

  timeout(10);
  drainJobQueue();
  if (result == "timed-out") {
    Atomics.add(i32, 0, 1);
  }
  result = "";

  test2()
    .then((v) => { result = v });
  drainJobQueue();
  if (result == "ok") {
    Atomics.add(i32, 0, 1);
  }
  result = "";

  test3()
    .then((v) => { result = v });
  drainJobQueue();
  if (result == "ok") {
    Atomics.add(i32, 0, 1);
  }
  result = "";

  result = test4();
  drainJobQueue();
  if (result == "timed-out") {
    Atomics.add(i32, 0, 1);
  }
	`);
}

// Custom Timeout to ensure that we wait until waitAsync times out.
function timeout(n) {
  var start = Date.now();
  while (Date.now() - start < n) {};
}

var result = "";
test1()
  .then((v) => { result = v });

timeout(10);
drainJobQueue();
assertEq(result, "timed-out");
result = "";

test2()
  .then((v) => { result = v });
drainJobQueue();
assertEq(result, "ok");
result = "";

test3()
  .then((v) => { result = v });
drainJobQueue();
assertEq(result, "ok");
result = "";

result = test4();
drainJobQueue();
assertEq(result, "timed-out");

test5();
let i32 = new Int32Array(sab);
while (Atomics.load(i32, 0) != 4) {};
