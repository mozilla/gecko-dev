const worker1 = new Worker("preview-multiple-threads-worker.js");
function fn1() {
  worker1.postMessage({ prop: true });
}
const worker2 = new Worker("preview-multiple-threads-worker.js");
function fn2() {
  worker2.postMessage({ prop: false });
}
