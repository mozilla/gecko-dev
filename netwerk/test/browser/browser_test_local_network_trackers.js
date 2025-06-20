/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { UrlClassifierTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlClassifierTestUtils.sys.mjs"
);

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const baseURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

const COLLECTION_NAME = "remote-permissions";
let rs = RemoteSettings(COLLECTION_NAME);

async function remoteSettingsSync({ created, updated, deleted }) {
  await rs.emit("sync", {
    data: {
      created,
      updated,
      deleted,
    },
  });
}

async function restorePermissions() {
  info("Restoring permissions");
  Services.obs.notifyObservers(null, "testonly-reload-permissions-from-disk");
  Services.perms.removeAll();
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["permissions.manager.defaultsUrl", ""],
      ["network.websocket.delay-failed-reconnects", false],
      ["network.websocket.max-connections", 1000],
    ],
  });

  // Make sure we start off "empty". Any RemoteSettings values must be
  // purged now to comply with test expectations.
  Services.obs.notifyObservers(null, "testonly-reload-permissions-from-disk");
  registerCleanupFunction(restorePermissions);
});

requestLongerTimeout(10);

function observeAndCheck(testType, rand, expectedStatus, message) {
  return new Promise(resolve => {
    let observer = {
      observe(subject, topic) {
        if (topic !== "http-on-stop-request") {
          return;
        }

        let url = `http://localhost:21555/?type=${testType}&rand=${rand}`;
        let channel = subject.QueryInterface(Ci.nsIHttpChannel);
        if (!channel || channel.URI.spec !== url) {
          return;
        }

        is(channel.status, expectedStatus, message);

        Services.obs.removeObserver(observer, "http-on-stop-request");
        resolve();
      },
    };
    Services.obs.addObserver(observer, "http-on-stop-request");
  });
}

let testCases = [
  {
    type: "fetch",
    nonTrackerStatus: Cr.NS_OK,
    trackerStatus: Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
  },
  {
    type: "xhr",
    nonTrackerStatus: Cr.NS_OK,
    trackerStatus: Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
  },
  {
    type: "img",
    nonTrackerStatus: Cr.NS_OK,
    trackerStatus: Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
  },
  {
    type: "video",
    nonTrackerStatus: Cr.NS_OK,
    trackerStatus: Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
  },
  {
    type: "audio",
    nonTrackerStatus: Cr.NS_OK,
    trackerStatus: Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
  },
  {
    type: "iframe",
    nonTrackerStatus: Cr.NS_OK,
    trackerStatus: Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
  },
  {
    type: "script",
    nonTrackerStatus: Cr.NS_OK,
    trackerStatus: Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
  },
  { type: "font", nonTrackerStatus: Cr.NS_OK, trackerStatus: Cr.NS_OK }, // TODO
  {
    type: "websocket",
    nonTrackerStatus: Cr.NS_ERROR_WEBSOCKET_CONNECTION_REFUSED,
    trackerStatus: Cr.NS_ERROR_LOCAL_NETWORK_ACCESS_DENIED,
  },
];

// Tests that a public->private fetch load initiated from
// a tracking script will fail.
add_task(async function test_tracker_initiated_lna_fetch() {
  let server = new HttpServer();
  server.start(21555);
  registerCleanupFunction(async () => {
    await server.stop();
  });
  server.registerPathHandler("/", (request, response) => {
    const params = new URLSearchParams(request.queryString);
    const type = params.get("type");

    response.setHeader("Access-Control-Allow-Origin", "*", false);

    switch (type) {
      case "img":
        response.setHeader("Content-Type", "image/gif", false);
        response.setStatusLine(request.httpVersion, 200, "OK");
        // 1x1 transparent GIF
        response.write(
          atob("R0lGODlhAQABAIAAAAAAAP///ywAAAAAAQABAAACAUwAOw==")
        );
        return;

      case "audio":
        response.setHeader("Content-Type", "audio/wav", false);
        response.setStatusLine(request.httpVersion, 200, "OK");
        // Silent WAV (44-byte header + no data)
        response.write(
          atob("UklGRhYAAABXQVZFZm10IBAAAAABAAEAIlYAAESsAAACABAAZGF0YQAAAAA=")
        );
        return;

      case "video":
        response.setHeader("Content-Type", "video/mp4", false);
        response.setStatusLine(request.httpVersion, 200, "OK");
        // Minimal MP4 file header; may not render but satisfies loader
        response.write(
          atob(
            "GkXfo0AgQoaBAUL3gQFC8oEEQvOBCEKCQAR3ZWJtQoeBAkKFgQIYU4BnQI0VSalmQCgq17FAAw9CQE2AQAZ3aGFtbXlXQUAGd2hhbW15RIlACECPQAAAAAAAFlSua0AxrkAu14EBY8WBAZyBACK1nEADdW5khkAFVl9WUDglhohAA1ZQOIOBAeBABrCBCLqBCB9DtnVAIueBAKNAHIEAAIAwAQCdASoIAAgAAUAmJaQAA3AA/vz0AAA="
          )
        );
        return;

      default:
        response.setHeader("Content-Type", "text/plain", false);
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.write("hello");
    }
  });

  for (let test of testCases) {
    let rand = Math.random();
    let promise = observeAndCheck(
      test.type,
      rand,
      test.nonTrackerStatus,
      `expected correct status for non-tracker ${test.type} test`
    );
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      baseURL + `page_with_trackers.html?test=${test.type}&rand=${rand}`
    );

    await promise;
    gBrowser.removeTab(tab);
  }

  registerCleanupFunction(UrlClassifierTestUtils.cleanupTestTrackers);
  await UrlClassifierTestUtils.addTestTrackers();

  for (let test of testCases) {
    let rand = Math.random();
    let promise = observeAndCheck(
      test.type,
      rand,
      test.trackerStatus,
      `expected correct status for tracker ${test.type} test`
    );
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      baseURL + `page_with_trackers.html?test=${test.type}&rand=${rand}`
    );

    await promise;
    gBrowser.removeTab(tab);
  }

  // check that when adding the permission the fetch req succeeds.
  PermissionTestUtils.add(
    baseURL + "page_with_trackers.html",
    "localhost",
    Services.perms.ALLOW_ACTION,
    Services.perms.EXPIRE_NEVER
  );

  for (let test of testCases) {
    let rand = Math.random();
    let promise = observeAndCheck(
      test.type,
      rand,
      test.nonTrackerStatus,
      `expected correct status for tracker ${test.type} test with permission`
    );
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      baseURL + `page_with_trackers.html?test=${test.type}&rand=${rand}`
    );

    await promise;
    gBrowser.removeTab(tab);
  }

  PermissionTestUtils.remove(baseURL + "page_with_trackers.html", "localhost");

  // This time check that the remote permission service can automatically set up the permission for this domain.
  const ORIGIN_1 = "https://example.com";
  const TEST_PERMISSION_1 = "localhost";
  await remoteSettingsSync({
    created: [
      {
        origin: ORIGIN_1,
        type: TEST_PERMISSION_1,
        capability: Ci.nsIPermissionManager.ALLOW_ACTION,
      },
    ],
  });

  for (let test of testCases) {
    let rand = Math.random();
    let promise = observeAndCheck(
      test.type,
      rand,
      test.nonTrackerStatus,
      `expected correct status for tracker ${test.type} test with permission from remote-settings`
    );
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      baseURL + `page_with_trackers.html?test=${test.type}&rand=${rand}`
    );

    await promise;
    gBrowser.removeTab(tab);
  }

  restorePermissions();
});
