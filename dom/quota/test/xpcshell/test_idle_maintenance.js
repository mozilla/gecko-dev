/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

/* exported testSteps */
async function testSteps() {
  info("Sending fake 'idle-daily' notification to QuotaManager");

  let observer = Services.qms.QueryInterface(Ci.nsIObserver);

  observer.observe(null, "idle-daily", "");

  info("Waiting for maintenance to start");

  await TestUtils.topicObserved("QuotaManager::MaintenanceStarted");
}
