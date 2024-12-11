export function checkFromESM(ok, is, finish) {
  let worker = new ChromeWorker("esm_url_worker.js");
  worker.onmessage = function (event) {
    if (event.data.type == "finish") {
      finish();
    } else if (event.data.type == "url") {
      URL.revokeObjectURL(event.data.url);
    } else if (event.data.type == "status") {
      ok(event.data.status, event.data.msg);
    }
  };

  worker.onerror = function (event) {
    is(event.target, worker);
    ok(false, "Worker had an error: " + event.data);
    worker.terminate();
    finish();
  };

  worker.postMessage(0);
}
