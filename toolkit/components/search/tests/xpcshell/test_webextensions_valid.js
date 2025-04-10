/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let extension;
let extensionPostData;
let oldRemoveEngineFunc;

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig([{ identifier: "unused" }]);

  await Services.search.init();
  await promiseAfterSettings();

  extension = await SearchTestUtils.installSearchExtension(
    {},
    { skipUnload: true }
  );
  extensionPostData = await SearchTestUtils.installSearchExtension(
    {
      name: "PostData",
      search_url_post_params: "?q={searchTerms}&post=1",
    },
    { skipUnload: true }
  );
  await extension.awaitStartup();
  await extensionPostData.awaitStartup();

  // For these tests, stub-out the removeEngine function, so that when we
  // remove it from the add-on manager, the engine is left in the search
  // settings.
  oldRemoveEngineFunc = Services.search.wrappedJSObject.removeEngine.bind(
    Services.search.wrappedJSObject
  );
  Services.search.wrappedJSObject.removeEngine = () => {};

  registerCleanupFunction(async () => {
    await extensionPostData.unload();
  });
});

add_task(async function test_valid_extensions_do_nothing() {
  Services.fog.testResetFOG();

  Assert.ok(
    Services.search.getEngineByName("Example"),
    "Should have installed the engine"
  );
  Assert.ok(
    !!Services.search.getEngineByName("PostData"),
    "Should have installed the PostData engine"
  );

  await Services.search.runBackgroundChecks();

  let labels = ["1", "2", "4", "5", "6"];
  for (let label of labels) {
    let recordedQuantity =
      Glean.browserSearchinit.engineInvalidWebextension[label].testGetValue();

    Assert.equal(
      recordedQuantity,
      null,
      `Should not have recorded any issues for label ${label}`
    );
  }
});

add_task(async function test_different_name() {
  Services.fog.testResetFOG();

  let engine = Services.search.getEngineByName("Example");

  engine.wrappedJSObject._name = "Example Test";

  await Services.search.runBackgroundChecks();

  let recordedQuantity =
    Glean.browserSearchinit.engineInvalidWebextension[
      extension.id
    ].testGetValue();

  Assert.equal(
    recordedQuantity,
    5,
    "Should record an invalid web extension because the addon has a different name"
  );

  engine.wrappedJSObject._name = "Example";
});

add_task(async function test_different_url() {
  Services.fog.testResetFOG();

  let engine = Services.search.getEngineByName("Example");

  engine.wrappedJSObject._urls = [];
  engine.wrappedJSObject._setUrls({
    search_url: "https://example.com/123",
    search_url_get_params: "?q={searchTerms}",
  });

  await Services.search.runBackgroundChecks();

  let recordedQuantity =
    Glean.browserSearchinit.engineInvalidWebextension[
      extension.id
    ].testGetValue();

  Assert.equal(
    recordedQuantity,
    6,
    "Should record an invalid web extension because the addon has a different submission url"
  );
});

add_task(async function test_different_url_post_data() {
  Services.fog.testResetFOG();

  let engine = Services.search.getEngineByName("PostData");

  engine.wrappedJSObject._urls = [];
  engine.wrappedJSObject._setUrls({
    search_url: "https://example.com/123",
    search_url_post_params: "?q={searchTerms}",
  });

  await Services.search.runBackgroundChecks();

  let recordedQuantity =
    Glean.browserSearchinit.engineInvalidWebextension[
      extensionPostData.id
    ].testGetValue();

  Assert.equal(
    recordedQuantity,
    6,
    "Should record an invalid web extension because the addon has different url POST data"
  );
});

add_task(async function test_extension_no_longer_specifies_engine() {
  Services.fog.testResetFOG();

  let extensionInfo = {
    useAddonManager: "permanent",
    manifest: {
      version: "2.0",
      browser_specific_settings: {
        gecko: {
          id: "example@tests.mozilla.org",
        },
      },
    },
  };

  await extension.upgrade(extensionInfo);

  await Services.search.runBackgroundChecks();

  let recordedQuantity =
    Glean.browserSearchinit.engineInvalidWebextension[
      extension.id
    ].testGetValue();

  Assert.equal(
    recordedQuantity,
    4,
    "Should record an invalid web extension because the search engine is no longer specified"
  );
});

add_task(async function test_disabled_extension() {
  // We don't reset Glean across tasks this time, ensuring the metric gets set
  // to the new value, rather than added.

  // Disable the extension, this won't remove the search engine because we've
  // stubbed removeEngine.
  await extension.addon.disable();

  await Services.search.runBackgroundChecks();

  let recordedQuantity =
    Glean.browserSearchinit.engineInvalidWebextension[
      extension.id
    ].testGetValue();

  Assert.equal(
    recordedQuantity,
    2,
    "Should record an invalid web extension because the addon is disabled"
  );

  extension.addon.enable();
  await extension.awaitStartup();
});

add_task(async function test_missing_extension() {
  // We don't reset Glean across tasks this time, ensuring the metric gets set
  // to the new value, rather than added.

  let extensionId = extension.id;
  // Remove the extension, this won't remove the search engine because we've
  // stubbed removeEngine.
  await extension.unload();

  await Services.search.runBackgroundChecks();

  let recordedQuantity =
    Glean.browserSearchinit.engineInvalidWebextension[
      extensionId
    ].testGetValue();

  Assert.equal(
    recordedQuantity,
    1,
    "Should record an invalid web extension because the addon is no longer installed"
  );

  await oldRemoveEngineFunc(Services.search.getEngineByName("Example"));
});
