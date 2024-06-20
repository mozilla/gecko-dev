/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testSteps */
async function testSteps() {
  info("Sending fake 'idle-daily' notification to QuotaManager");

  let observer = Services.qms.QueryInterface(Ci.nsIObserver);

  observer.observe(null, "idle-daily", "");

  info("Waiting for maintenance to start");

  await new Promise(function (resolve) {
    Services.obs.addObserver(function observer(subject, topic) {
      Services.obs.removeObserver(observer, topic);
      resolve();
    }, "QuotaManager::MaintenanceStarted");
  });
}
