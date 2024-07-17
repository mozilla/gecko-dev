/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gBaseUrl;

async function getEngineNames() {
  let engines = await Services.search.getEngines();
  return engines.map(engine => engine._name);
}

add_setup(async function () {
  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");
  gBaseUrl = `http://localhost:${server.identity.primaryPort}/`;

  await SearchTestUtils.useTestEngines("test-extensions");

  Services.locale.availableLocales = [
    ...Services.locale.availableLocales,
    "af",
  ];

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("browser.search.region");
  });
});

add_task(async function basic_install_test() {
  await Services.search.init();
  await promiseAfterSettings();

  // On first boot, we get the configuration defaults
  Assert.deepEqual(await getEngineNames(), ["Plain", "Special"]);

  // User installs a new search engine
  let extension = await SearchTestUtils.installSearchExtension(
    {
      encoding: "windows-1252",
    },
    { skipUnload: true }
  );
  Assert.deepEqual((await getEngineNames()).sort(), [
    "Example",
    "Plain",
    "Special",
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
  Assert.deepEqual(await getEngineNames(), ["Plain", "Special"]);
});

add_task(async function test_install_duplicate_engine() {
  let name = "Plain";
  consoleAllowList.push(`An engine called ${name} already exists`);
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name,
      search_url: "https://example.com/plain",
    },
    { skipUnload: true }
  );

  let engine = await Services.search.getEngineByName("Plain");
  let submission = engine.getSubmission("foo");
  Assert.equal(
    submission.uri.spec,
    "https://duckduckgo.com/?t=ffsb&q=foo",
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
      favicon_url: `${gBaseUrl}/head_search.js`,
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
      favicon_url: `${gDataUrl}/iconsRedirect.sjs?type=invalid`,
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
      favicon_url: `${gDataUrl}/iconsRedirect.sjs`,
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
