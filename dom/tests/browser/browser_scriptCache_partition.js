requestLongerTimeout(2);

const TEST_SCRIPT_URL_0 =
  "https://example.com/browser/dom/tests/browser/page_scriptCache_partition.html";

const TEST_SCRIPT_URL_1 =
  "https://example.org/browser/dom/tests/browser/page_scriptCache_partition.html";

const TEST_MODULE_URL_0 =
  "https://example.com/browser/dom/tests/browser/page_scriptCache_partition_module.html";

const TEST_MODULE_URL_1 =
  "https://example.org/browser/dom/tests/browser/page_scriptCache_partition_module.html";

const TEST_SJS_URL =
  "https://example.net/browser/dom/tests/browser/counter_server.sjs";

async function testScriptCacheAndPartition({
  enableCache,
  type,
}) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.script_loader.navigation_cache", enableCache],
    ],
  });
  registerCleanupFunction(() => SpecialPowers.popPrefEnv());

  const response1 = await fetch(TEST_SJS_URL + "?reset");
  is(await response1.text(), "reset", "Server state should be reset");

  ChromeUtils.clearResourceCache();
  Services.cache2.clear();

  async function getCounter() {
    return SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      const iframe = content.document.querySelector("iframe");
      return SpecialPowers.spawn(iframe, [], () => {
        return content.document.body.getAttribute("counter");
      });
    });
  }

  async function load(url) {
    const loadedPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    tab.linkedBrowser.loadURI(Services.io.newURI(url), {
      triggeringPrincipal: tab.linkedBrowser.nodePrincipal,
    });
    await loadedPromise;
  }

  // Loading script from the different top-level frame domain should use
  // separate cache for each partition, if the partitioning is enabled.
  //
  // Each top-level document are loaded into separate processes, say,
  // process A for url0 (example.com), and process B for url1
  // (example.org).
  //
  // If Fission is enabled, the iframe (example.net) is loaded into yet another
  // process C, both with the url0 load and the url1 load.
  // In this case, they share single SharedScriptCache instance.
  // If partitioning is enabled, the cache for the script loaded by the iframe
  // should be separated for each.
  // If partitioning is disabled, the cache for the script loaded by the iframe
  // should be shared.
  //
  // If Fission is not enabled, the iframe is loaded into process A and B,
  // and they don't share the cache entry in SharedScriptCache.
  //
  // If navigation cache is not enabled, then the partitioning is done by
  // the Necko side, and the Necko cache should also be partitioned.
  const url0 = type === "script" ? TEST_SCRIPT_URL_0 : TEST_MODULE_URL_0;
  const url1 = type === "script" ? TEST_SCRIPT_URL_1 : TEST_MODULE_URL_1;

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: url0,
  });
  is(await getCounter(), "0");

  await load(url1);
  is(await getCounter(), "1");

  // Reloading the page should use the cached script, for each partition.
  await load(url0);
  is(await getCounter(), "0");

  await load(url1);
  is(await getCounter(), "1");

  BrowserTestUtils.removeTab(tab);
}

add_task(async function testScriptNoCachePartition() {
  await testScriptCacheAndPartition({
    enableCache: false,
    type: "script",
  });
});

add_task(async function testScriptCachePartition() {
  await testScriptCacheAndPartition({
    enableCache: true,
    type: "script",
  });
});

add_task(async function testModuleNoCachePartition() {
  await testScriptCacheAndPartition({
    enableCache: false,
    type: "module",
  });
});

add_task(async function testModuleCachePartition() {
  await testScriptCacheAndPartition({
    enableCache: true,
    type: "module",
  });
});
