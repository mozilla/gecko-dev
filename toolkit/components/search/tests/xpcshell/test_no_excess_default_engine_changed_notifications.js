/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Ensures we don't generate a Glean search.default.engine.changed event at
 * startup for a search-related addon that the user has already installed.
 */

"use strict";

const kSearchEngineURL = "https://example.com/?q={searchTerms}&foo=myparams";

const CONFIG = [
  {
    identifier: "MozParamsTest1",
    base: {
      name: "MozParamsTest1",
      urls: {
        search: {
          base: "https://example.com/",
          params: [
            {
              name: "simple1",
              value: "5",
            },
          ],
          searchTermParamName: "q",
        },
      },
    },
  },
  {
    identifier: "MozParamsTest2",
    base: {
      name: "MozParamsTest2",
      urls: {
        search: {
          base: "https://example.com/",
          params: [
            {
              name: "simple2",
              value: "5",
            },
          ],
          searchTermParamName: "q",
        },
      },
    },
  },
];

let notificationSpy;

add_setup(async function () {
  notificationSpy = sinon.spy(SearchUtils, "notifyAction");

  await SearchTestUtils.setRemoteSettingsConfig(CONFIG);
  await SearchTestUtils.initXPCShellAddonManager();
  Services.search.wrappedJSObject.reset();
  await Services.search.init();

  registerCleanupFunction(async () => {
    sinon.restore();
  });
});

add_task(
  async function test_correct_default_engine_changed_event_count_with_search_addon() {
    const settings = await RemoteSettings(SearchUtils.SETTINGS_ALLOWLIST_KEY);
    sinon.stub(settings, "get").returns([
      {
        thirdPartyId: "test@thirdparty.example.com",
        overridesAppIdv2: "MozParamsTest2",
        urls: [
          {
            search_url: "https://example.com/?q={searchTerms}&foo=myparams",
          },
        ],
      },
    ]);

    let ext = ExtensionTestUtils.loadExtension({
      manifest: {
        browser_specific_settings: {
          gecko: {
            id: "test@thirdparty.example.com",
          },
        },
        chrome_settings_overrides: {
          search_provider: {
            is_default: true,
            name: "MozParamsTest2",
            keyword: "MozSearch",
            search_url: kSearchEngineURL,
          },
        },
      },
      useAddonManager: "permanent",
    });

    await ext.startup();
    await AddonTestUtils.waitForSearchProviderStartup(ext);
    await promiseAfterSettings();

    let engines = await Services.search.getEngines();
    let overriddenEngine = engines.find(e => e.name == "MozParamsTest2");

    Assert.equal(
      notificationSpy.withArgs(overriddenEngine, "engine-changed").callCount,
      1,
      "Should have sent 1 'engine-changed' notification"
    );

    notificationSpy.resetHistory();
    Services.search.wrappedJSObject.reset();
    await Services.search.init();
    await AddonTestUtils.promiseRestartManager();
    await ext.awaitStartup();

    let engineAfterRestart = Services.search.getEngineByName("MozParamsTest2");

    Assert.equal(
      engineAfterRestart.wrappedJSObject.getAttr("overriddenBy"),
      "test@thirdparty.example.com",
      "After restart, the MozParamsTest2 engine should have an 'overriddenBy' property"
    );

    Assert.equal(
      notificationSpy.withArgs(
        engineAfterRestart.wrappedJSObject,
        "engine-changed"
      ).callCount,
      0,
      "Should not have sent a new notification after restarting the Search Service"
    );

    await ext.unload();
  }
);
