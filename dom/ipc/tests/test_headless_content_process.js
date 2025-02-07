"use strict";

const TEST_REMOTE_TYPE = "test";

function allTestProcs() {
  return ChromeUtils.getAllDOMProcesses().filter(
    p => p.remoteType == TEST_REMOTE_TYPE
  );
}

add_task(async function headlessContentProcessKeepAlive() {
  let testProcesses = allTestProcs();
  equal(testProcesses.length, 0);

  // Start the process, it should appear in the list.
  let keepAlive =
    await ChromeUtils.ensureHeadlessContentProcess(TEST_REMOTE_TYPE);
  let domProcess = keepAlive.domProcess;

  testProcesses = allTestProcs();
  equal(testProcesses.length, 1);
  equal(testProcesses[0], domProcess);
  ok(domProcess.canSend);

  // The process should be properly kept alive, so `releaseCachedProcesses()`
  // should not shut it down.
  Services.ppmm.releaseCachedProcesses();

  testProcesses = allTestProcs();
  equal(testProcesses.length, 1);
  equal(testProcesses[0], domProcess);
  ok(domProcess.canSend);

  // Invalidating the keep alive should lead to the process being shut down.
  keepAlive.invalidateKeepAlive();

  // Wait for the process to be shut down.
  await new Promise(resolve => {
    Services.obs.addObserver(function obs(subject, topic) {
      equal(topic, "ipc:content-shutdown");
      if (
        subject.QueryInterface(Ci.nsIPropertyBag2).getProperty("childID") ==
        domProcess.childID
      ) {
        Services.obs.removeObserver(obs, "ipc:content-shutdown");
        resolve();
      }
    }, "ipc:content-shutdown");
  });

  testProcesses = allTestProcs();
  equal(testProcesses.length, 0);
  ok(!domProcess.canSend);
});
