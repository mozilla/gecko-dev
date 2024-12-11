/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that when an additional URL is added to a
 * default override allowlist entry, we correctly handle updating
 * add-ons with the new URL.
 */

"use strict";

const BASE_URL = "https://example.com/";
const INITIAL_SEARCH_ENGINE_URL = `${BASE_URL}?q={searchTerms}&initial=true`;
const UPDATED_SEARCH_ENGINE_URL = `${BASE_URL}?q={searchTerms}&initial=false`;

const ENGINE_NAME = "Simple Engine";
const EXTENSION_ID = "test@thirdparty.example.com";

const allowlist = [
  {
    thirdPartyId: EXTENSION_ID,
    overridesAppIdv2: "simple",
    urls: [
      {
        search_url: INITIAL_SEARCH_ENGINE_URL,
      },
    ],
  },
];

let extension;
let extensionInfo;

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig([
    { identifier: "originalDefault" },
    {
      identifier: "simple",
      base: { name: ENGINE_NAME },
    },
  ]);
  await SearchTestUtils.initXPCShellAddonManager();
  await Services.search.init();

  extensionInfo = {
    startupReason: "ADDON_INSTALL",
    useAddonManager: "permanent",
    id: EXTENSION_ID,
    manifest: {
      browser_specific_settings: {
        gecko: {
          id: EXTENSION_ID,
        },
      },
      chrome_settings_overrides: {
        search_provider: {
          is_default: true,
          name: ENGINE_NAME,
          keyword: "MozSearch",
          search_url: INITIAL_SEARCH_ENGINE_URL,
        },
      },
    },
  };

  extension = ExtensionTestUtils.loadExtension(extensionInfo);

  registerCleanupFunction(async () => {
    await extension.unload();
    sinon.restore();
  });

  const settings = await RemoteSettings(SearchUtils.SETTINGS_ALLOWLIST_KEY);
  sinon.stub(settings, "get").returns(allowlist);
});

add_task(async function update_allowlist_and_addon() {
  await extension.startup();

  let result = await Services.search.maybeSetAndOverrideDefault(extensionInfo);

  Assert.equal(
    result.canChangeToAppProvided,
    true,
    "Should have returned the correct value for allowing switch to default or not."
  );

  let engine = await Services.search.getEngineByName(ENGINE_NAME);
  let submission = engine.getSubmission("{searchTerms}");
  Assert.equal(
    decodeURI(submission.uri.spec),
    INITIAL_SEARCH_ENGINE_URL,
    "Should have applied the correct url from the add-on."
  );

  // Add a different URL to the allowlist.
  allowlist[0].urls.push({
    search_url: UPDATED_SEARCH_ENGINE_URL,
  });

  let promiseChanged = TestUtils.topicObserved(
    "browser-search-engine-modified",
    (eng, verb) => verb == "engine-changed"
  );

  // Update the search add-on with the different URL.
  extensionInfo.manifest.chrome_settings_overrides.search_provider.search_url =
    UPDATED_SEARCH_ENGINE_URL;
  await extension.upgrade(extensionInfo);

  await AddonTestUtils.waitForSearchProviderStartup(extension);
  await promiseChanged;

  // Confirm that search engine from the add-on is still the default.
  Assert.equal(
    Services.search.defaultEngine.name,
    engine.name,
    "Engine should still be default."
  );

  // Verify that search engine uses the new URL from the add-on.
  submission = engine.getSubmission("{searchTerms}");
  Assert.equal(
    decodeURI(submission.uri.spec),
    UPDATED_SEARCH_ENGINE_URL,
    "Should have applied the correct url from the add-on."
  );
});
