/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper_service_workers_navigation.js */
loadHelperScript("helper_service_workers_navigation.js");

const COM_PAGE_URL = URL_ROOT_SSL + "test_sw_page.html";
const COM_WORKER_URL = URL_ROOT_SSL + "test_sw_page_worker.js";

// Extracted from browser_target_command_service_workers_navigation for frequent
// failures.
add_task(async function test_NavigationToPageWithExistingStoppedWorker() {
  await setupServiceWorkerNavigationTest();

  const tab = await addTab(COM_PAGE_URL);

  info("Wait until the service worker registration is registered");
  await waitForRegistrationReady(tab, COM_PAGE_URL, COM_WORKER_URL);

  await stopServiceWorker(COM_WORKER_URL);

  const { hooks, commands, targetCommand } =
    await watchServiceWorkerTargets(tab);

  // Let some time to watch target to eventually regress and revive the worker
  await wait(1000);

  // As the Service Worker doesn't have any active worker... it doesn't report any target.
  info(
    "Verify that no SW is reported after it has been stopped and we start watching for service workers"
  );
  await checkHooks(hooks, {
    available: 0,
    destroyed: 0,
    targets: [],
  });

  info("Reload the worker module via the postMessage call");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    const registration = await content.wrappedJSObject.registrationPromise;
    // Force loading the worker again, even it has been stopped
    registration.active.postMessage("");
  });

  info("Verify that the SW is notified");
  await checkHooks(hooks, {
    available: 1,
    destroyed: 0,
    targets: [COM_WORKER_URL],
  });

  await unregisterServiceWorker(COM_WORKER_URL);

  await checkHooks(hooks, {
    available: 1,
    destroyed: 1,
    targets: [],
  });

  // Stop listening to avoid worker related requests
  targetCommand.destroy();

  await commands.waitForRequestsToSettle();
  await commands.destroy();
  await removeTab(tab);
});
