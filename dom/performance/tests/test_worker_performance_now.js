function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + ": " + msg + "\n");
  postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function workerTestDone() {
  postMessage({ type: 'finish' });
}

function workerTestGetOSCPU(cb) {
  addEventListener('message', function workerTestGetOSCPUCB(e) {
    if (e.data.type !== 'returnOSCPU') {
      return;
    }
    removeEventListener('message', workerTestGetOSCPUCB);
    cb(e.data.result);
  });
  postMessage({
    type: 'getOSCPU'
  });
}

ok(self.performance, "Performance object should exist.");
ok(typeof self.performance.now == 'function', "Performance object should have a 'now' method.");
var n = self.performance.now(), d = Date.now();
ok(n >= 0, "The value of now() should be equal to or greater than 0.");
ok(self.performance.now() >= n, "The value of now() should monotonically increase.");

workerTestDone();

