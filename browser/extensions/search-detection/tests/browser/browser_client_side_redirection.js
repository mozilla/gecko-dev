/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

SearchTestUtils.init(this);

const TELEMETRY_EVENTS_FILTERS = {
  category: "addonsSearchDetection",
  method: "etld_change",
};

// The search-detection built-in add-on records events in the parent process.
const TELEMETRY_TEST_UTILS_OPTIONS = { clear: true, process: "parent" };

async function testClientSideRedirect({
  background,
  permissions,
  telemetryExpected = false,
  redirectingAppProvidedEngine = false,
}) {
  Services.fog.testResetFOG();
  Services.telemetry.clearEvents();

  // Load an extension that does a client-side redirect. We expect this
  // extension to be reported in a Telemetry event when `telemetryExpected` is
  // set to `true`.
  const addonId = "some@addon-id";
  const addonVersion = "1.2.3";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      version: addonVersion,
      browser_specific_settings: { gecko: { id: addonId } },
      permissions,
    },
    useAddonManager: "temporary",
    background,
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  // Simulate a search (with the test search engine) by navigating to it.
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: redirectingAppProvidedEngine
        ? "https://example.org/default?q=babar"
        : "https://example.com/search?q=babar",
    },
    () => {}
  );

  await extension.unload();

  TelemetryTestUtils.assertEvents(
    telemetryExpected
      ? [
          {
            object: "webrequest",
            value: "extension",
            extra: {
              addonId,
              addonVersion,
              from: redirectingAppProvidedEngine
                ? "example.org"
                : "example.com",
              to: "mochi.test",
            },
          },
        ]
      : [],
    TELEMETRY_EVENTS_FILTERS,
    TELEMETRY_TEST_UTILS_OPTIONS
  );

  let events = Glean.addonsSearchDetection.etldChangeWebrequest.testGetValue();
  if (!telemetryExpected) {
    Assert.equal(null, events);
  } else {
    Assert.equal(1, events.length);
    Assert.equal("extension", events[0].extra.value);
    Assert.equal(addonId, events[0].extra.addonId);
    Assert.equal(addonVersion, events[0].extra.addonVersion);
    Assert.equal(
      redirectingAppProvidedEngine ? "example.org" : "example.com",
      events[0].extra.from
    );
    Assert.equal("mochi.test", events[0].extra.to);
  }
}

add_setup(async function () {
  const searchEngineName = "test search engine";

  await SearchTestUtils.updateRemoteSettingsConfig([
    {
      identifier: "default",
      base: {
        urls: {
          search: {
            base: "https://example.org/default",
            searchTermParamName: "q",
          },
        },
      },
    },
  ]);

  await SearchTestUtils.installSearchExtension({
    name: searchEngineName,
    keyword: "test",
    search_url: "https://example.com/?q={searchTerms}",
  });

  Assert.ok(
    !!Services.search.getEngineByName(searchEngineName),
    "test search engine registered"
  );
});

add_task(function test_onBeforeRequest() {
  return testClientSideRedirect({
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        () => {
          return {
            redirectUrl: "http://mochi.test:8888/",
          };
        },
        { urls: ["*://example.com/*"] },
        ["blocking"]
      );

      browser.test.sendMessage("ready");
    },
    permissions: ["webRequest", "webRequestBlocking", "*://example.com/*"],
    telemetryExpected: true,
  });
});

add_task(function test_onBeforeRequest_appProvidedEngine() {
  return testClientSideRedirect({
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        () => {
          return {
            redirectUrl: "http://mochi.test:8888/",
          };
        },
        { urls: ["*://example.org/*"] },
        ["blocking"]
      );

      browser.test.sendMessage("ready");
    },
    permissions: ["webRequest", "webRequestBlocking", "*://example.org/*"],
    redirectingAppProvidedEngine: true,
    telemetryExpected: true,
  });
});

add_task(function test_onBeforeRequest_url_not_monitored() {
  // Here, we load an extension that does a client-side redirect. Because this
  // extension does not listen to the URL of the search engine registered
  // above, we don't expect this extension to be reported in a Telemetry event.
  return testClientSideRedirect({
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        () => {
          return {
            redirectUrl: "http://mochi.test:8888/",
          };
        },
        { urls: ["*://google.com/*"] },
        ["blocking"]
      );

      browser.test.sendMessage("ready");
    },
    permissions: ["webRequest", "webRequestBlocking", "*://google.com/*"],
    telemetryExpected: false,
  });
});

add_task(function test_onHeadersReceived() {
  return testClientSideRedirect({
    background() {
      browser.webRequest.onHeadersReceived.addListener(
        () => {
          return {
            redirectUrl: "http://mochi.test:8888/",
          };
        },
        { urls: ["*://example.com/*"], types: ["main_frame"] },
        ["blocking"]
      );

      browser.test.sendMessage("ready");
    },
    permissions: ["webRequest", "webRequestBlocking", "*://example.com/*"],
    telemetryExpected: true,
  });
});

add_task(function test_onHeadersReceived_appProvidedEngine() {
  return testClientSideRedirect({
    background() {
      browser.webRequest.onHeadersReceived.addListener(
        () => {
          return {
            redirectUrl: "http://mochi.test:8888/",
          };
        },
        { urls: ["*://example.org/*"], types: ["main_frame"] },
        ["blocking"]
      );

      browser.test.sendMessage("ready");
    },
    permissions: ["webRequest", "webRequestBlocking", "*://example.org/*"],
    redirectingAppProvidedEngine: true,
    telemetryExpected: true,
  });
});

add_task(function test_onHeadersReceived_url_not_monitored() {
  // Here, we load an extension that does a client-side redirect. Because this
  // extension does not listen to the URL of the search engine registered
  // above, we don't expect this extension to be reported in a Telemetry event.
  return testClientSideRedirect({
    background() {
      browser.webRequest.onHeadersReceived.addListener(
        () => {
          return {
            redirectUrl: "http://mochi.test:8888/",
          };
        },
        { urls: ["*://google.com/*"], types: ["main_frame"] },
        ["blocking"]
      );

      browser.test.sendMessage("ready");
    },
    permissions: ["webRequest", "webRequestBlocking", "*://google.com/*"],
    telemetryExpected: false,
  });
});
