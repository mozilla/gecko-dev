// |jit-test| --setpref=atomics_wait_async=true; skip-if: helperThreadCount() === 0;

// Test timing out.
function test1() {
  var o = {};
  o.sab = new SharedArrayBuffer(4096);
  o.ia = new Int32Array(o.sab);
  o.ia[37] = 0x1337;

  var promise = Atomics.waitAsync(o.ia, 37, 0x1337, 10).value;
  assertEq(!!o.sab, true);
  gc();
  return promise
    .then((v) => { assertEq(v, "timed-out"); })
    .catch((v) => { assertEq(false, true); })
}
// Test wait and notify.
function test2() {
  var o = {};
  o.sab = new SharedArrayBuffer(4096);
  o.ia = new Int32Array(o.sab);
  o.ia[37] = 0x1337;

  var promise = Atomics.waitAsync(o.ia, 37, 0x1337, 100).value;
  assertEq(!!o.sab, true);
  gc();
  Atomics.notify(o.ia, 37);
  return promise
    .then((v) => { assertEq(v, "ok"); })
    .catch((v) => { assertEq(false, true) })
}

// Test no timeout and notify.
function test3() {
  var o = {};
  o.sab = new SharedArrayBuffer(4096);
  o.ia = new Int32Array(o.sab);
  o.ia[37] = 0x1337;

  var promise = Atomics.waitAsync(o.ia, 37, 0x1337).value;
  assertEq(!!o.sab, true);
  gc();
  Atomics.notify(o.ia, 37);
  return promise
    .then((v) => { assertEq(v, "ok"); })
    .catch((v) => { assertEq(false, true) })
}

// Test immediate resolution.
function test4() {
  var o = {};
  o.sab = new SharedArrayBuffer(4096);
  o.ia = new Int32Array(o.sab);
  o.ia[37] = 0x1337;

  var v = Atomics.waitAsync(o.ia, 37, 0x1337, 0).value;
  assertEq(!!o.sab, true);
  gc();
  assertEq(v, "timed-out");
}

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

    var promise = Atomics.waitAsync(o.ia, 37, 0x1337, 10).value;
    assertEq(!!o.sab, true);
    gc();
    return promise
      .then((v) => { assertEq(v, "timed-out"); })
      .catch((v) => { assertEq(false, true); })
  }
  function test2() {
    var o = {};
    o.sab = new SharedArrayBuffer(4096);
    o.ia = new Int32Array(o.sab);
    o.ia[37] = 0x1337;

    var promise = Atomics.waitAsync(o.ia, 37, 0x1337, 10).value;
    assertEq(!!o.sab, true);
    gc();
    Atomics.notify(o.ia, 37);
    return promise
      .then((v) => { assertEq(v, "ok"); })
      .catch((v) => { assertEq(false, true )})
  }
  function test3() {
    var o = {};
    o.sab = new SharedArrayBuffer(4096);
    o.ia = new Int32Array(o.sab);
    o.ia[37] = 0x1337;

    var promise = Atomics.waitAsync(o.ia, 37, 0x1337).value;
    assertEq(!!o.sab, true);
    gc();
    Atomics.notify(o.ia, 37);
    return promise
      .then((v) => { assertEq(v, "ok"); })
      .catch((v) => { assertEq(false, true) })
  }
  function test4() {
    var o = {};
    o.sab = new SharedArrayBuffer(4096);
    o.ia = new Int32Array(o.sab);
    o.ia[37] = 0x1337;

    var v = Atomics.waitAsync(o.ia, 37, 0x1337, 0).value;
    assertEq(!!o.sab, true);
    gc();
    assertEq(v, "timed-out");
  }
  test1()
    .then(() => test2())
    .then(() => test3())
    .then(() => test4())
    .then(() => {
      Atomics.store(i32, 0, 1);
    });
	`);
}

test1()
  .then(() => test2())
  .then(() => test3())
  .then(() => test4())
  .then(() => test5())
  .then(() => {
  let i32 = new Int32Array(sab);
  while (Atomics.load(i32, 0) === 0) {}
  assertEq(true,true);
})
