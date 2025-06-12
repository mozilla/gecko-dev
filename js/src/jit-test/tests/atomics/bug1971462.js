const sab = new SharedArrayBuffer(4);
const i32a = new Int32Array(sab);

const veryLargeMs = 9.223372036854776e12;
Atomics.wait(i32a, 0, 0, veryLargeMs);
