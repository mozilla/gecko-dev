// |jit-test| --setpref=atomics_wait_async=true; skip-if: helperThreadCount() === 0;

var sab = new SharedArrayBuffer(4096);
var ia = new Int32Array(sab);
ia[37] = 0x1337;

Atomics.waitAsync(ia, 37, 0x1337, 1000);
