/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  UrlbarSearchTermsPersistence:
    "resource:///modules/UrlbarSearchTermsPersistence.sys.mjs",
});

const TEST_PROVIDER_INFO = [
  {
    id: "example",
    queryParamNames: ["q"],
    searchPageRegexp: "^https://www\\.example\\.com/",
    includeParams: [
      {
        key: "page",
        values: ["web"],
        canBeMissing: true,
      },
    ],
    excludeParams: [
      {
        key: "excludeKey",
        values: ["hello"],
      },
    ],
  },
];

const RECORDS = {
  current: TEST_PROVIDER_INFO,
  created: [],
  updated: TEST_PROVIDER_INFO,
  deleted: [],
};

const URLBAR_PERSISTENCE_SETTINGS_KEY = "urlbar-persisted-search-terms";

const client = RemoteSettings(URLBAR_PERSISTENCE_SETTINGS_KEY);

let defaultTestEngine;

async function updateClientWithRecords(records) {
  let promise = TestUtils.topicObserved("urlbar-persisted-search-terms-synced");

  await client.emit("sync", { data: records });

  info("Wait for UrlbarSearchTermsPersistence component to update.");
  await promise;
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });
  let cleanup = await installPersistTestEngines();
  defaultTestEngine = Services.search.getEngineByName("Example");
  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
    // Clear existing records.
    await updateClientWithRecords({
      current: [],
      created: [],
      updated: [],
      deleted: [],
    });
    cleanup();
  });
});

add_task(async function test_remote_settings_sync() {
  let [searchUrl] = UrlbarUtils.getSearchQueryUrl(defaultTestEngine, "foobar");
  await BrowserTestUtils.withNewTab(searchUrl, async function (browser) {
    Assert.equal(
      window.gURLBar.value,
      "foobar",
      "Search string should be the urlbar value."
    );

    let webUrl = searchUrl + "&page=web";
    await SpecialPowers.spawn(browser, [webUrl], async function (url) {
      content.history.pushState(null, "", url);
    });

    Assert.equal(
      window.gURLBar.value,
      "foobar",
      "Search string should still be the urlbar value."
    );

    let nonWebUrl = searchUrl + "&excludeKey=hello";
    await SpecialPowers.spawn(browser, [nonWebUrl], async function (url) {
      content.history.pushState(null, "", url);
    });
    Assert.equal(
      window.gURLBar.value,
      "foobar",
      "Search string should still be the urlbar value."
    );
  });

  // Sync with Remote Settings
  await updateClientWithRecords(RECORDS);

  await BrowserTestUtils.withNewTab(searchUrl, async function (browser) {
    Assert.equal(
      window.gURLBar.value,
      "foobar",
      "Search string should be the urlbar value."
    );

    let webUrl = searchUrl + "&page=web";
    await SpecialPowers.spawn(browser, [webUrl], async function (url) {
      content.history.pushState(null, "", url);
    });
    Assert.equal(
      window.gURLBar.value,
      "foobar",
      "Search string should still be the urlbar value."
    );

    let nonWebUrl = searchUrl + "&excludeKey=hello";

    await SpecialPowers.spawn(browser, [nonWebUrl], async function (url) {
      content.history.pushState(null, "", url);
    });

    Assert.equal(
      window.gURLBar.value,
      UrlbarTestUtils.trimURL(nonWebUrl),
      "Search string should not be the urlbar value."
    );
  });
});
