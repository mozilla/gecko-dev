const TEST_URL =
  "https://example.com/browser/dom/tests/browser/page_scriptCache_load_events.html";

function clearAllCache() {
  return new Promise(function (resolve) {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_ALL_CACHES,
      resolve
    );
  });
}

async function testOrder() {
  await clearAllCache();

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  // The script should be executed in between DOMContentLoaded and load events.

  // Uncached cache.
  let result = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return content.document.getElementById("result").textContent;
  });
  is(result, "DOMContentLoaded+script+load");

  await BrowserTestUtils.reloadTab(tab);

  // Cached cache.
  result = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return content.document.getElementById("result").textContent;
  });
  is(result, "DOMContentLoaded+script+load");

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_withoutNavigationCache() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.script_loader.navigation_cache", false]],
  });
  registerCleanupFunction(() => SpecialPowers.popPrefEnv());

  await testOrder();
});

add_task(async function test_withNavigationCache() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.script_loader.navigation_cache", true]],
  });
  registerCleanupFunction(() => SpecialPowers.popPrefEnv());

  await testOrder();
});
