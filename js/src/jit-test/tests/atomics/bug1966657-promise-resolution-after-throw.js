// |jit-test| skip-if: helperThreadCount() === 0

let sab = new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT);
setSharedObject(sab);

evalInWorker(`
  const i32 = new Int32Array(getSharedObject());
  let p = new Promise((resolve) => resolve(3));
  p.then(() => {
    Atomics.store(i32, 0, 1);
  });
  throw "error";
`);


let i32 = new Int32Array(sab);
while (Atomics.load(i32, 0) === 0) {}

