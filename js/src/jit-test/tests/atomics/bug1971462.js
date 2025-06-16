// |jit-test| skip-if: !this.SharedArrayBuffer || helperThreadCount() === 0

const sab = new SharedArrayBuffer(4);
const i32a = new Int32Array(sab);
setSharedObject(sab);

evalInWorker(`
  const sab = getSharedObject();
  const i32a = new Int32Array(sab);

  Atomics.store(i32a, 0, 1);

  const veryLargeMs = 9.223372036854776e12;
  print(Atomics.wait(i32a, 0, 1, veryLargeMs));
  Atomics.store(i32a, 0, 2);
`);

while (Atomics.load(i32a, 0) == 0) {}

while (Atomics.load(i32a, 0) == 1) {
  Atomics.notify(i32a, 0);
}
