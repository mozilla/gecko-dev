/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function getEngineNames() {
  let engines = await Services.search.getEngines();
  return engines.map(engine => engine._name);
}

add_setup(async function () {
  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");

  SearchTestUtils.setRemoteSettingsConfig([
    {
      identifier: "default",
      base: {
        urls: {
          search: {
            base: "https://example.com/unchanged",
            searchTermParamName: "q",
          },
        },
      },
    },
    { identifier: "additional" },
  ]);

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.search.region");
  });
});

add_task(async function basic_install_test() {
  await Services.search.init();
  await promiseAfterSettings();

  // On first boot, we get the configuration defaults
  Assert.deepEqual(await getEngineNames(), ["default", "additional"]);

  // User installs a new search engine
  let extension = await SearchTestUtils.installSearchExtension(
    {
      encoding: "windows-1252",
    },
    { skipUnload: true }
  );
  Assert.deepEqual((await getEngineNames()).sort(), [
    "Example",
    "additional",
    "default",
  ]);

  let engine = await Services.search.getEngineByName("Example");
  Assert.equal(
    engine.wrappedJSObject.queryCharset,
    "windows-1252",
    "Should have the correct charset"
  );

  // User uninstalls their engine
  await extension.awaitStartup();
  await extension.unload();
  await promiseAfterSettings();
  Assert.deepEqual(await getEngineNames(), ["default", "additional"]);
});

add_task(async function test_install_duplicate_engine() {
  let name = "default";
  consoleAllowList.push(`An engine called ${name} already exists`);
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name,
      search_url: "https://example.com/plain",
    },
    { skipUnload: true }
  );

  let engine = await Services.search.getEngineByName("default");
  let submission = engine.getSubmission("foo");
  Assert.equal(
    submission.uri.spec,
    "https://example.com/unchanged?q=foo",
    "Should have not changed the app provided engine."
  );

  // User uninstalls their engine
  await extension.unload();
});

add_task(async function test_load_favicon_invalid() {
  let observed = TestUtils.consoleMessageObserved(msg => {
    return msg.wrappedJSObject.arguments[0].includes(
      "Content type does not match expected"
    );
  });

  // User installs a new search engine
  let extension = await SearchTestUtils.installSearchExtension(
    {
      favicon_url: `${gHttpURL}/head_search.js`,
    },
    { skipUnload: true }
  );

  await observed;

  let engine = await Services.search.getEngineByName("Example");
  Assert.equal(
    null,
    await engine.getIconURL(),
    "Should not have set an iconURI"
  );

  // User uninstalls their engine
  await extension.awaitStartup();
  await extension.unload();
  await promiseAfterSettings();
});

add_task(async function test_load_favicon_invalid_redirect() {
  let observed = TestUtils.consoleMessageObserved(msg => {
    return msg.wrappedJSObject.arguments[0].includes(
      "Content type does not match expected"
    );
  });

  // User installs a new search engine
  let extension = await SearchTestUtils.installSearchExtension(
    {
      favicon_url: `${gHttpURL}/sjs/iconsRedirect.sjs?type=invalid`,
    },
    { skipUnload: true }
  );

  await observed;

  let engine = await Services.search.getEngineByName("Example");
  Assert.equal(
    null,
    await engine.getIconURL(),
    "Should not have set an iconURI"
  );

  // User uninstalls their engine
  await extension.awaitStartup();
  await extension.unload();
  await promiseAfterSettings();
});

add_task(async function test_load_favicon_redirect() {
  let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  // User installs a new search engine
  let extension = await SearchTestUtils.installSearchExtension(
    {
      favicon_url: `${gHttpURL}/sjs/iconsRedirect.sjs`,
    },
    { skipUnload: true }
  );

  let engine = await Services.search.getEngineByName("Example");

  await promiseIconChanged;

  Assert.ok(await engine.getIconURL(), "Should have set an iconURI");
  Assert.ok(
    (await engine.getIconURL()).startsWith("data:image/x-icon;base64,"),
    "Should have saved the expected content type for the icon"
  );

  // User uninstalls their engine
  await extension.awaitStartup();
  await extension.unload();
  await promiseAfterSettings();
});

// The size of the favicon_url is not explicitly stated in the manifest
// so we test if it is detected correctly for various file and url types.
add_task(async function test_load_icon_extension_url_ico() {
  let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  let response = await fetch(`${gHttpURL}/icons/remoteIcon.ico`);
  let iconBuffer = await response.arrayBuffer();

  let extension = await SearchTestUtils.installSearchExtension(
    {
      favicon_url: "icon.ico", // 16x16
      icons: {
        // These icons are not loaded because they have an explicit size.
        48: "icon2.png",
        49: "icon3.png",
      },
    },
    { skipUnload: true },
    { "icon.ico": iconBuffer }
  );

  let engine = Services.search.getEngineByName("Example");
  await promiseIconChanged;
  let iconMapObj = engine.wrappedJSObject._iconMapObj;

  Assert.deepEqual(
    Object.keys(iconMapObj).toSorted(),
    ["16", "48", "49"],
    "Should have the correct 3 icons."
  );

  Assert.equal(
    await engine.getIconURL(16),
    `moz-extension://${extension.uuid}/icon.ico`,
    "16x16 icon is correct."
  );

  Assert.equal(
    await engine.getIconURL(48),
    `moz-extension://${extension.uuid}/icon2.png`,
    "48x48 icon is correct."
  );
  Assert.equal(
    await engine.getIconURL(49),
    `moz-extension://${extension.uuid}/icon3.png`,
    "49x49 icon is correct."
  );
  Assert.equal(
    await engine.getIconURL(50),
    `moz-extension://${extension.uuid}/icon3.png`,
    "Uses 49x49 icon for size 50."
  );

  // User uninstalls their engine
  await extension.awaitStartup();
  await extension.unload();
  await promiseAfterSettings();
});

add_task(async function test_load_icon_extension_url_svg() {
  let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  let response = await fetch(`${gHttpURL}/icons/svgIcon.svg`);
  let iconBuffer = await response.arrayBuffer();

  let extension = await SearchTestUtils.installSearchExtension(
    {
      favicon_url: "icon.svg", // 16x16.
      icons: {
        // These icons are not loaded because they have an explicit size.
        48: "icon2.png",
        49: "icon3.png",
      },
    },
    { skipUnload: true },
    { "icon.svg": iconBuffer }
  );

  let engine = Services.search.getEngineByName("Example");
  await promiseIconChanged;
  let iconMapObj = engine.wrappedJSObject._iconMapObj;

  Assert.deepEqual(
    Object.keys(iconMapObj).toSorted(),
    ["16", "48", "49"],
    "Should have the correct 3 icons."
  );

  Assert.equal(
    await engine.getIconURL(16),
    `moz-extension://${extension.uuid}/icon.svg`,
    "16x16 icon is correct."
  );

  // User uninstalls their engine
  await extension.awaitStartup();
  await extension.unload();
  await promiseAfterSettings();
});

add_task(async function test_load_icon_http_url_ico() {
  let promiseIconChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ICON_CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  let extension = await SearchTestUtils.installSearchExtension(
    {
      favicon_url: `${gHttpURL}/sjs/iconsRedirect.sjs`, //16x16
      // These icons are not loaded because they have an explicit size.
      icons: {
        48: "icon.png",
        49: "icon2.png",
      },
    },
    { skipUnload: true }
  );

  let engine = Services.search.getEngineByName("Example");
  await promiseIconChanged;
  let iconMapObj = engine.wrappedJSObject._iconMapObj;

  Assert.deepEqual(
    Object.keys(iconMapObj).toSorted(),
    ["16", "48", "49"],
    "Should have the correct 3 icons."
  );
  Assert.ok(
    iconMapObj[16].startsWith("data:image/x-icon;base64,"),
    "Should have saved the expected content type for the 16x16 icon"
  );

  // User uninstalls their engine
  await extension.awaitStartup();
  await extension.unload();
  await promiseAfterSettings();
});
