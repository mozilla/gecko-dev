/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests ensures that Search Access Point (SAP) telemetry is reported
 * correctly when an application provided search engine is overridden by a
 * third party add-on.
 */

"use strict";

add_setup(async function () {
  let settings = RemoteSettings(SearchUtils.SETTINGS_ALLOWLIST_KEY);
  sinon.stub(settings, "get").returns([
    {
      thirdPartyId: "thirdparty@tests.mozilla.org",
      overridesAppIdv2: "overridden",
      urls: [
        {
          search_url: "https://example.org/search",
          search_url_get_params: "?pc=thirdparty&q={searchTerms}",
        },
      ],
    },
  ]);

  await SearchTestUtils.updateRemoteSettingsConfig([
    {
      identifier: "originalDefault",
      base: {
        urls: {
          search: {
            base: "https://example.com/search",
            params: [{ name: "pc", value: "{partnerCode}" }],
            searchTermParamName: "q",
          },
        },
        partnerCode: "original-default",
      },
    },
    {
      identifier: "overridden",
      base: {
        name: "engine-override",
        urls: {
          search: {
            base: "https://example.org/search",
            params: [{ name: "pc", value: "{partnerCode}" }],
            searchTermParamName: "q",
          },
        },
        partnerCode: "overridden-base",
      },
    },
  ]);

  registerCleanupFunction(async () => {
    sinon.restore();
  });
});

async function searchWithDefault() {
  Services.fog.testResetFOG();
  TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gURLBar.focus();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "heuristicResult",
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "Should be of type search"
  );

  let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  EventUtils.synthesizeMouseAtCenter(element, {});
  await loadPromise;
  BrowserTestUtils.removeTab(tab);
  await UrlbarTestUtils.formHistory.clear();
}

add_task(async function test_search_with_app_default_engine() {
  await searchWithDefault();

  await SearchUITestUtils.assertSAPTelemetry({
    engineId: "originalDefault",
    engineName: "originalDefault",
    extensionOverridden: false,
    partnerCode: "original-default",
    source: "urlbar",
    count: 1,
  });
});

add_task(async function test_search_with_app_engine_overridden() {
  await SearchTestUtils.installSearchExtension({
    id: "thirdparty@tests.mozilla.org",
    name: "engine-override",
    is_default: true,
    search_url: "https://example.org/search",
    search_url_get_params: "?pc=thirdparty&q={searchTerms}",
  });

  await searchWithDefault();

  await SearchUITestUtils.assertSAPTelemetry({
    engineId: "overridden",
    engineName: "engine-override",
    overriddenByThirdParty: true,
    // Partner code is not specified for overridden engines, since we do not
    // need to report it (and it would require parsing the URL).
    source: "urlbar",
    count: 1,
  });
});
