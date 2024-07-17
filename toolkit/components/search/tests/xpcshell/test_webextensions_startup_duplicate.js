/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExtensionTestUtils:
    "resource://testing-common/ExtensionXPCShellUtils.sys.mjs",
});

add_setup(async function () {
  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");
  await SearchTestUtils.useTestEngines("test-extensions");
  await SearchTestUtils.initXPCShellAddonManager();
});

add_task(async function test_install_duplicate_engine_startup() {
  let name = "Plain";
  consoleAllowList.push("#createAndAddAddonEngine failed for");
  // Do not use SearchTestUtils.installSearchExtension, as we need to manually
  // start the search service after installing the extension.
  let extensionInfo = {
    useAddonManager: "permanent",
    files: {},
    manifest: SearchTestUtils.createEngineManifest({
      name,
      search_url: "https://example.com/plain",
    }),
  };

  let extension = lazy.ExtensionTestUtils.loadExtension(extensionInfo);
  await extension.startup();

  await Services.search.init();

  await AddonTestUtils.waitForSearchProviderStartup(extension);
  let engine = await Services.search.getEngineByName(name);
  let submission = engine.getSubmission("foo");
  Assert.equal(
    submission.uri.spec,
    "https://duckduckgo.com/?t=ffsb&q=foo",
    "Should have not changed the app provided engine."
  );

  await extension.unload();
});
