/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

const RustFutures = ChromeUtils.importESModule(
  "resource://gre/modules/RustFutures.sys.mjs"
);

RustFutures.initializeGeckoGlobalWorkerQueue();

add_task(async function simpleTest() {
  const tester = RustFutures.FutureTester.init();
  const f = tester.makeFuture();
  shortDelay().then(() => runCompleteFutures(tester, 42, 1));
  Assert.equal(await f, 42);
});

async function runCompleteFutures(tester, value, futureCount) {
  var completedCount = 0;
  // Call completeFutures in a loop until we've completed the expected number of futures.
  // Since makeFuture is an async call, we need to make sure that completeFutures runs after all
  // the futures have been made and inserted into the internal vec.
  for (;;) {
    completedCount += tester.completeFutures(value);
    if (completedCount >= futureCount) {
      return;
    }
    await shortDelay();
  }
}

add_task(async function twoFutures() {
  const tester = RustFutures.FutureTester.init();
  // Create a future and complete it using an async task
  const f1 = tester.makeFuture();
  const f2 = tester.makeFuture();
  runCompleteFutures(tester, 84, 2);
  Assert.deepEqual(await Promise.all([f1, f2]), [84, 84]);
});

add_task(async function roundtripFunctions() {
  Assert.equal(await RustFutures.roundtripU8(42), 42);
  Assert.equal(await RustFutures.roundtripI8(-42), -42);
  Assert.equal(await RustFutures.roundtripU16(42), 42);
  Assert.equal(await RustFutures.roundtripI16(-42), -42);
  Assert.equal(await RustFutures.roundtripU32(42), 42);
  Assert.equal(await RustFutures.roundtripI32(-42), -42);
  Assert.equal(await RustFutures.roundtripU64(42), 42);
  Assert.equal(await RustFutures.roundtripI64(-42), -42);
  Assert.equal(await RustFutures.roundtripF32(0.5), 0.5);
  Assert.equal(await RustFutures.roundtripF64(-0.5), -0.5);
  Assert.equal(await RustFutures.roundtripString("hi"), "hi");
  Assert.deepEqual(await RustFutures.roundtripVec([42]), [42]);
  Assert.deepEqual(await RustFutures.roundtripMap({ hello: "world" }), {
    hello: "world",
  });
  const obj = RustFutures.Traveller.init("Alice");
  Assert.equal((await RustFutures.roundtripObj(obj)).name(), "Alice");
});

add_task(async function wakeWhenNotReady() {
  const tester = RustFutures.FutureTester.init();
  var isResolved = false;
  const f = tester.makeFuture().then(value => {
    isResolved = true;
    return value;
  });
  // This will wake up the Rust wakers, but while the futures are still not ready
  // The JS promises should stay unresolved
  tester.wakeFutures();
  await shortDelay();
  Assert.equal(isResolved, false);
  // Okay, now let test revolving the futures
  runCompleteFutures(tester, 42, 1);
  Assert.equal(await f, 42);
  Assert.equal(isResolved, true);
});

add_task(async function testWorkerQueue() {
  Assert.equal(await RustFutures.expensiveComputation(), 1000);
});

// Utility function that sleeps for 10ms.
async function shortDelay() {
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 10));
}
