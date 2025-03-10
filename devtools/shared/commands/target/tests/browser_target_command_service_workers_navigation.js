/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper_service_workers_navigation.js */
loadHelperScript("helper_service_workers_navigation.js");

// Test the TargetCommand API for service workers when navigating in content tabs.
// When the top level target navigates, we manually call onTargetAvailable for
// service workers which now match the page domain. We assert that the callbacks
// will be called the expected number of times here.

const COM_PAGE_URL = URL_ROOT_SSL + "test_sw_page.html";
const COM_WORKER_URL = URL_ROOT_SSL + "test_sw_page_worker.js";
const ORG_PAGE_URL = URL_ROOT_ORG_SSL + "test_sw_page.html";
const ORG_WORKER_URL = URL_ROOT_ORG_SSL + "test_sw_page_worker.js";

/**
 * This test will navigate between two pages, both controlled by different
 * service workers.
 *
 * The steps will be:
 * - navigate to .com page
 * - create target list
 *   -> onAvailable should be called for the .com worker
 * - navigate to .org page
 *   -> onAvailable should be called for the .org worker
 * - reload .org page
 *   -> nothing should happen
 * - unregister .org worker
 *   -> onDestroyed should be called for the .org worker
 * - navigate back to .com page
 *   -> nothing should happen
 * - unregister .com worker
 *   -> onDestroyed should be called for the .com worker
 */
add_task(async function test_NavigationBetweenTwoDomains_NoDestroy() {
  await setupServiceWorkerNavigationTest();

  const tab = await addTab(COM_PAGE_URL);

  const { hooks, commands, targetCommand } =
    await watchServiceWorkerTargets(tab);

  // We expect onAvailable to have been called one time, for the only service
  // worker target available in the test page.
  await checkHooks(hooks, {
    available: 1,
    destroyed: 0,
    targets: [COM_WORKER_URL],
  });

  info("Go to .org page, wait for onAvailable to be called");
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    ORG_PAGE_URL
  );
  await checkHooks(hooks, {
    available: 2,
    destroyed: 0,
    targets: [COM_WORKER_URL, ORG_WORKER_URL],
  });

  info("Reload .org page, onAvailable and onDestroyed should not be called");
  await BrowserTestUtils.reloadTab(gBrowser.selectedTab);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 0,
    targets: [COM_WORKER_URL, ORG_WORKER_URL],
  });

  info("Unregister .org service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(ORG_WORKER_URL);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 1,
    targets: [COM_WORKER_URL],
  });

  info("Go back to .com page");
  const onBrowserLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    COM_PAGE_URL
  );
  await onBrowserLoaded;
  await checkHooks(hooks, {
    available: 2,
    destroyed: 1,
    targets: [COM_WORKER_URL],
  });

  info("Unregister .com service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(COM_WORKER_URL);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 2,
    targets: [],
  });

  // Stop listening to avoid worker related requests
  targetCommand.destroy();

  await commands.waitForRequestsToSettle();
  await commands.destroy();
  await removeTab(tab);
});

/**
 * In this test we load a service worker in a page prior to starting the
 * TargetCommand. We start the target list on another page, and then we go back to
 * the first page. We want to check that we are correctly notified about the
 * worker that was spawned before TargetCommand.
 *
 * Steps:
 * - navigate to .com page
 * - navigate to .org page
 * - create target list
 *   -> onAvailable is called for the .org worker
 * - unregister .org worker
 *   -> onDestroyed is called for the .org worker
 * - navigate back to .com page
 *   -> onAvailable is called for the .com worker
 * - unregister .com worker
 *   -> onDestroyed is called for the .com worker
 */
add_task(async function test_NavigationToPageWithExistingWorker() {
  await setupServiceWorkerNavigationTest();

  const tab = await addTab(COM_PAGE_URL);

  info("Wait until the service worker registration is registered");
  await waitForRegistrationReady(tab, COM_PAGE_URL, COM_WORKER_URL);

  info("Navigate to another page");
  let onBrowserLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser
  );
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    ORG_PAGE_URL
  );

  // Avoid TV failures, where target list still starts thinking that the
  // current domain is .com .
  info("Wait until we have fully navigated to the .org page");
  // wait for the browser to be loaded otherwise the task spawned in waitForRegistrationReady
  // might be destroyed (when it still belongs to the previous content process)
  await onBrowserLoaded;
  await waitForRegistrationReady(tab, ORG_PAGE_URL, ORG_WORKER_URL);

  const { hooks, commands, targetCommand } =
    await watchServiceWorkerTargets(tab);

  // We expect onAvailable to have been called one time, for the only service
  // worker target available in the test page.
  await checkHooks(hooks, {
    available: 1,
    destroyed: 0,
    targets: [ORG_WORKER_URL],
  });

  info("Unregister .org service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(ORG_WORKER_URL);
  await checkHooks(hooks, {
    available: 1,
    destroyed: 1,
    targets: [],
  });

  info("Go back .com page, wait for onAvailable to be called");
  onBrowserLoaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    COM_PAGE_URL
  );
  await onBrowserLoaded;

  await checkHooks(hooks, {
    available: 2,
    destroyed: 1,
    targets: [COM_WORKER_URL],
  });

  info("Unregister .com service worker and wait until onDestroyed is called.");
  await unregisterServiceWorker(COM_WORKER_URL);
  await checkHooks(hooks, {
    available: 2,
    destroyed: 2,
    targets: [],
  });

  // Stop listening to avoid worker related requests
  targetCommand.destroy();

  await commands.waitForRequestsToSettle();
  await commands.destroy();
  await removeTab(tab);
});
