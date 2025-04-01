const TEST_URL =
  "https://example.com/browser/dom/tests/browser/page_evict_with_non_cacheable.html";

const TEST_SCRIPT_URL =
  "https://example.com/browser/dom/tests/browser/cacheable_non_cacheable_server.sjs";

function clearAllCache() {
  return new Promise(function (resolve) {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_ALL_CACHES,
      resolve
    );
  });
}

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.script_loader.navigation_cache", true]],
  });
  registerCleanupFunction(() => SpecialPowers.popPrefEnv());

  // Set the server to return a cacheable script.
  const response1 = await fetch(TEST_SCRIPT_URL + "?use-cacheable");
  is(await response1.text(), "ok", "Server state should be set");

  await clearAllCache();

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  // The server should return a script that sets cacheable attribute to
  // true.
  let cacheable = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return content.document.body.getAttribute("cacheable");
  });
  is(cacheable, "true");

  // Reloading the page should use the cached script.
  await BrowserTestUtils.reloadTab(tab);
  cacheable = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return content.document.body.getAttribute("cacheable");
  });
  is(cacheable, "true");

  // Set the server to return a non-cacheable script.
  const response2 = await fetch(TEST_SCRIPT_URL + "?use-non-cacheable");
  is(await response2.text(), "ok", "Server state should be set");

  // Force-reload should fetch the script from the server
  await BrowserTestUtils.reloadTab(tab, { bypassCache: true });

  // The server should return a script that sets cacheable attribute to
  // false.
  cacheable = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return content.document.body.getAttribute("cacheable");
  });
  is(cacheable, "false");

  // Reloading the page shouldn't use the cached script.
  await BrowserTestUtils.reloadTab(tab);
  cacheable = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return content.document.body.getAttribute("cacheable");
  });
  is(cacheable, "false");

  BrowserTestUtils.removeTab(tab);
});
