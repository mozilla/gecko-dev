const TEST_URL = "https://example.com/browser/dom/tests/browser/dummy.html";
const SCRIPT_NAME = "counter_server.sjs";
const TEST_SCRIPT_URL =
  "https://example.com/browser/dom/tests/browser/" + SCRIPT_NAME;

function getCounter(tab) {
  const browser = tab.linkedBrowser;
  return SpecialPowers.spawn(browser, [SCRIPT_NAME], async scriptName => {
    const { promise, resolve } = Promise.withResolvers();

    const script = content.document.createElement("script");
    script.src = scriptName;
    script.addEventListener("load", resolve);
    content.document.body.appendChild(script);

    await promise;

    return parseInt(content.document.body.getAttribute("counter"));
  });
}

async function reloadAndGetCounter(tab) {
  await BrowserTestUtils.reloadTab(tab);

  return getCounter(tab);
}

function clearAllCache() {
  return new Promise(function (resolve) {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_ALL_CACHES,
      resolve
    );
  });
}

function clearJSCache() {
  return new Promise(function (resolve) {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_JS_CACHE,
      resolve
    );
  });
}

function clearNetworkCache() {
  return new Promise(function (resolve) {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_NETWORK_CACHE,
      resolve
    );
  });
}

add_task(async function test_withoutNavigationCache() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.script_loader.navigation_cache", false]],
  });
  registerCleanupFunction(() => SpecialPowers.popPrefEnv());

  await clearAllCache();

  const resetResponse = await fetch(TEST_SCRIPT_URL + "?reset");
  is(await resetResponse.text(), "reset", "Server state should be reset");

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  is(await getCounter(tab), 0, "counter should be 0 for the first load.");

  is(
    await reloadAndGetCounter(tab),
    0,
    "cache should be used for subsequent load."
  );

  await clearAllCache();
  is(
    await reloadAndGetCounter(tab),
    1,
    "request should reach the server after removing all cache."
  );
  is(
    await reloadAndGetCounter(tab),
    1,
    "cache should be used for subsequent load."
  );

  await clearJSCache();
  is(await reloadAndGetCounter(tab), 1, "network cache should be used.");

  await clearNetworkCache();
  is(
    await reloadAndGetCounter(tab),
    2,
    "request should reach the server after network cache is cleared."
  );

  await clearJSCache();
  is(await reloadAndGetCounter(tab), 2, "network cache should be used.");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_withNavigationCache() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.script_loader.navigation_cache", true]],
  });
  registerCleanupFunction(() => SpecialPowers.popPrefEnv());

  await clearAllCache();

  const resetResponse = await fetch(TEST_SCRIPT_URL + "?reset");
  is(await resetResponse.text(), "reset", "Server state should be reset");

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  is(await getCounter(tab), 0, "counter should be 0 for the first load.");

  is(
    await reloadAndGetCounter(tab),
    0,
    "cache should be used for subsequent load."
  );

  await clearAllCache();
  is(
    await reloadAndGetCounter(tab),
    1,
    "request should reach the server after removing all cache."
  );
  is(
    await reloadAndGetCounter(tab),
    1,
    "cache should be used for subsequent load."
  );

  await clearJSCache();
  is(await reloadAndGetCounter(tab), 1, "network cache should be used.");

  // The above reload loads from the network cache, and the JS cache is
  // re-created.

  await clearNetworkCache();
  is(await reloadAndGetCounter(tab), 1, "JS cache should be used.");

  // The above reload loads from the JS cache.
  // Currently, network cache is not re-created from the JS cache.

  await clearJSCache();
  is(
    await reloadAndGetCounter(tab),
    2,
    "request should reach the server after both cache is cleared."
  );

  BrowserTestUtils.removeTab(tab);
});
