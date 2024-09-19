/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-serviceworker.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-serviceworker.js",
  this
);
/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

const SW_TAB_URL = URL_ROOT_SSL + "resources/service-workers/push-sw.html";
const SW_URL = URL_ROOT_SSL + "resources/service-workers/push-sw.worker.js";

/**
 * Test that service workers' origin attributes are displayed in the runtime page.
 */
add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.firstparty.isolate", true]],
  });

  prepareCollapsibilitiesTest();
  await enableServiceWorkerDebugging();
  const { document, tab, window } = await openAboutDebugging({
    enableWorkerUpdates: true,
  });
  const store = window.AboutDebugging.store;

  await selectThisFirefoxPage(document, store);

  // open a tab and register service worker
  info("Register a service worker");
  const swTab = await addTab(SW_TAB_URL);

  // check that service worker is rendered
  info("Wait until the service worker appears and is running");
  await waitForServiceWorkerRunning(SW_URL, document);

  let swPane = getDebugTargetPane("Service Workers", document);
  Assert.strictEqual(
    swPane.querySelectorAll(".qa-debug-target-item").length,
    1,
    "Service worker list has one element"
  );
  const url = new URL(SW_URL);
  const firstPartyAttribute = url.origin + "^firstPartyDomain=" + url.hostname;
  ok(
    swPane
      .querySelector(".qa-debug-target-item")
      .textContent.includes(firstPartyAttribute),
    "Service worker list displays the origin information correctly"
  );

  // unregister the service worker
  info("Unregister service worker");
  await unregisterServiceWorker(swTab);
  // check that service worker is not rendered anymore
  info("Wait for service worker to disappear");
  await waitUntil(() => {
    swPane = getDebugTargetPane("Service Workers", document);
    return swPane.querySelectorAll(".qa-debug-target-item").length === 0;
  });

  info("Remove tabs");
  await removeTab(swTab);
  await removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
