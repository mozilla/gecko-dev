// |jit-test| --enable-atomics-pause; skip-if: !Atomics.pause || helperThreadCount() === 0 || getBuildConfiguration("arm64-simulator") === true

function startWorker(worker) {
  evalInWorker(`
    (${worker})(getSharedObject());
  `);
}

// Index 0: Worker Lock
// Index 1: Counter
// Index 2: Sync
// Index 3: Worker State
let sab = new SharedArrayBuffer(4 * Int32Array.BYTES_PER_ELEMENT)
let i32 = new Int32Array(sab);

setSharedObject(sab);

// Number of workers.
const N = 4;

// Number of iterations.
const K = N * 1000;

for (let i = 0; i < N; ++i) {
  startWorker(function(sab) {
    // Number of workers.
    const N = 4;

    // Number of iterations.
    const K = N * 1000;

    let i32 = new Int32Array(sab);

    // Mark worker as started.
    Atomics.add(i32, 3, 1);

    // Wait until main thread is ready.
    Atomics.wait(i32, 2, 0);

    for (let i = 0; i < K / N; ++i) {
      // Spin-wait loop using a "test, test-and-set" technique.
      while (true) {
        while (Atomics.load(i32, 0) !== 0) {
          Atomics.pause();
        }
        if (Atomics.exchange(i32, 0, 1) === 0) {
          break;
        }
      }

      // "Critical section" - non-atomic load-and-store.
      i32[1] += 1;

      // Leave "Critical section".
      Atomics.store(i32, 0, 0);
    }

    // Mark worker as finished.
    Atomics.sub(i32, 3, 1);
  });
}

// Wait until all worker threads have started.
while (Atomics.load(i32, 3) !== N) {
  Atomics.pause();
}

// Start work in all worker threads.
let woken = 0;
while ((woken += Atomics.notify(i32, 2, N)) !== N) {
}

// Wait until all worker threads have finished.
while (Atomics.load(i32, 3) !== 0) {
  Atomics.pause();
}

assertEq(i32[1], K);
