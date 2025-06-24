/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1973042 - Tests that ensure that worker fetches for tracker domains are
 * properly blocked by tracking protection.
 */

"use strict";

const TEST_TRACKER_DOMAIN = "https://tracking.example.com/";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.trackingprotection.enabled", true]],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(_ => {
    UrlClassifierTestUtils.cleanupTestTrackers();
  });
});

add_task(async function test_workerFetch() {
  // Open a tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE_HTTPS
  );

  // Create a dedicated worker on the page and instruct it to fetch.
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [
      TEST_TRACKER_DOMAIN + TEST_PATH + "corsAllowed.html",
      TEST_4TH_PARTY_DOMAIN_HTTPS + TEST_PATH + "corsAllowed.html",
    ],
    async (trackingUrl, nonTrackingUrl) => {
      let worker = new content.Worker("workerFetch.js");

      // Send a fetch request to a tracking domain.
      let result = await new content.Promise(resolve => {
        worker.addEventListener("message", function handler(e) {
          if (e.data.type === "FetchResult") {
            if (e.data.success) {
              resolve(true);
            } else {
              resolve(false);
            }
            worker.removeEventListener("message", handler);
          }
        });

        // Send the fetch request to the worker
        worker.postMessage({ type: "Fetch", url: trackingUrl });
      });

      // The fetch should be blocked by tracking protection
      ok(
        !result,
        "Dedicated worker fetch to a tracker domain should be blocked by tracking protection"
      );

      // Send a fetch request to a non-tracking domain.
      result = await new content.Promise(resolve => {
        worker.addEventListener("message", function handler(e) {
          if (e.data.type === "FetchResult") {
            if (e.data.success) {
              resolve(true);
            } else {
              resolve(false);
            }
            worker.removeEventListener("message", handler);
          }
        });

        worker.postMessage({ type: "Fetch", url: nonTrackingUrl });
      });

      // The fetch should succeed
      ok(
        result,
        "Dedicated worker fetch to non-tracking domain should succeed"
      );

      // Terminate the worker
      worker.terminate();
    }
  );

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_sharedWorkerFetch() {
  // Open a tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE_HTTPS
  );

  // Create a shared worker on the page and instruct it to fetch
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [
      TEST_TRACKER_DOMAIN + TEST_PATH + "corsAllowed.html",
      TEST_4TH_PARTY_DOMAIN_HTTPS + TEST_PATH + "corsAllowed.html",
    ],
    async (trackingUrl, nonTrackingUrl) => {
      let worker = new content.SharedWorker("sharedWorkerFetch.js");

      // Create a promise that will resolve when we get the response
      let result = await new content.Promise(resolve => {
        worker.port.addEventListener("message", function handler(e) {
          if (e.data.type === "FetchResult") {
            if (e.data.success) {
              resolve(true);
            } else {
              resolve(false);
            }
            worker.port.removeEventListener("message", handler);
          }
        });

        // Start the port connection
        worker.port.start();

        // Send the fetch request to the worker
        worker.port.postMessage({ type: "Fetch", url: trackingUrl });
      });

      // The fetch should be blocked by tracking protection
      ok(
        !result,
        "Shared worker fetch to a tracker domain should be blocked by tracking protection"
      );

      // Send a fetch request to a non-tracking domain.
      result = await new content.Promise(resolve => {
        worker.port.addEventListener("message", function handler(e) {
          if (e.data.type === "FetchResult") {
            if (e.data.success) {
              resolve(true);
            } else {
              resolve(false);
            }
            worker.port.removeEventListener("message", handler);
          }
        });

        // Send the fetch request to the worker
        worker.port.postMessage({ type: "Fetch", url: nonTrackingUrl });
      });

      // The fetch should succeed
      ok(result, "Shared worker fetch to non-tracking domain should succeed");

      // Terminate the worker
      worker.port.close();
    }
  );

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_serviceWorkerFetch() {
  // Open a tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE_HTTPS
  );

  // Register a service worker on the page and instruct it to fetch from a
  // tracker domain.
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [
      TEST_TRACKER_DOMAIN + TEST_PATH + "corsAllowed.html",
      TEST_4TH_PARTY_DOMAIN_HTTPS + TEST_PATH + "corsAllowed.html",
    ],
    async (trackingUrl, nonTrackingUrl) => {
      let reg = await content.navigator.serviceWorker.register(
        "serviceWorkerFetch.js"
      );

      if (reg.installing.state !== "activated") {
        await new content.Promise(resolve => {
          let w = reg.installing;
          w.addEventListener("statechange", function onStateChange() {
            if (w.state === "activated") {
              w.removeEventListener("statechange", onStateChange);
              resolve();
            }
          });
        });
      }

      // Create a promise that will resolve when we get the response
      let result = await new content.Promise(resolve => {
        content.navigator.serviceWorker.addEventListener(
          "message",
          function handler(e) {
            if (e.data.type === "FetchResult") {
              if (e.data.success) {
                resolve(true);
              } else {
                resolve(false);
              }
              content.navigator.serviceWorker.removeEventListener(
                "message",
                handler
              );
            }
          }
        );

        reg.active.postMessage({ type: "Fetch", url: trackingUrl });
      });

      ok(
        !result,
        "Service worker fetch to a tracker domain should be blocked by tracking protection"
      );

      result = await new content.Promise(resolve => {
        content.navigator.serviceWorker.addEventListener(
          "message",
          function handler(e) {
            if (e.data.type === "FetchResult") {
              if (e.data.success) {
                resolve(true);
              } else {
                resolve(false);
              }
              content.navigator.serviceWorker.removeEventListener(
                "message",
                handler
              );
            }
          }
        );

        reg.active.postMessage({ type: "Fetch", url: nonTrackingUrl });
      });

      ok(
        result,
        "Service worker fetch to a non-tracking domain should succeed."
      );

      await reg.unregister();
    }
  );

  BrowserTestUtils.removeTab(tab);
});
