const TEST_URL =
  "https://example.com/browser/dom/tests/browser/page_scriptCache_container.html";

const TEST_SCRIPT_URL =
  "https://example.com/browser/dom/tests/browser/counter_server.sjs";

async function testScriptCache({ enableCache }) {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.script_loader.navigation_cache", enableCache]],
  });
  registerCleanupFunction(() => SpecialPowers.popPrefEnv());

  const response1 = await fetch(TEST_SCRIPT_URL + "?reset");
  is(await response1.text(), "reset", "Server state should be reset");

  ChromeUtils.clearResourceCache();
  Services.cache2.clear();

  async function getCounter(tab) {
    return SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      return content.document.body.getAttribute("counter");
    });
  }

  async function openTab(url, userContextId) {
    const tab = BrowserTestUtils.addTab(gBrowser, url, { userContextId });
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    return tab;
  }

  // Loading script from different containers should use separate cache.
  const tab0 = await openTab(TEST_URL, 1);
  is(await getCounter(tab0), "0");

  const tab1 = await openTab(TEST_URL, 2);
  is(await getCounter(tab1), "1");

  // Reloading the page should use the cached script, for each container.
  await BrowserTestUtils.reloadTab(tab0);
  is(await getCounter(tab0), "0");

  await BrowserTestUtils.reloadTab(tab1);
  is(await getCounter(tab1), "1");

  BrowserTestUtils.removeTab(tab0);
  BrowserTestUtils.removeTab(tab1);
}

add_task(async function testNoCache() {
  await testScriptCache({
    enableCache: false,
  });
});

add_task(async function testCache() {
  await testScriptCache({
    enableCache: true,
  });
});
