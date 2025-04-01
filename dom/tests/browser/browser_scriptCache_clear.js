requestLongerTimeout(2);

const TEST_URL = "https://example.com/browser/dom/tests/browser/dummy.html";
const SCRIPT_NAME = "counter_server.sjs";
const TEST_SCRIPT_URL =
  "https://example.com/browser/dom/tests/browser/" + SCRIPT_NAME;

function getCounter(tab, type) {
  const browser = tab.linkedBrowser;
  return SpecialPowers.spawn(
    browser,
    [SCRIPT_NAME, type],
    async (scriptName, type) => {
      const { promise, resolve } = Promise.withResolvers();

      const script = content.document.createElement("script");
      switch (type) {
        case "script":
          script.addEventListener("load", resolve);
          script.src = scriptName;
          break;
        case "module-top-level":
          script.addEventListener("load", resolve);
          script.type = "module";
          script.src = scriptName;
          break;
        case "module-static":
          content.document.addEventListener("module-loaded", resolve);
          script.type = "module";
          script.textContent = `
import "./${scriptName}";
document.dispatchEvent(new CustomEvent("module-loaded"));
`;
          break;
        case "module-dynamic":
          content.document.addEventListener("module-loaded", resolve);
          script.type = "module";
          script.textContent = `
await import("./${scriptName}");
document.dispatchEvent(new CustomEvent("module-loaded"));
`;
          break;
      }
      content.document.body.appendChild(script);

      await promise;

      return parseInt(content.document.body.getAttribute("counter"));
    }
  );
}

async function reloadAndGetCounter(tab, type) {
  await BrowserTestUtils.reloadTab(tab);

  return getCounter(tab, type);
}

async function doTest(useNavigationCache, type) {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.script_loader.navigation_cache", useNavigationCache]],
  });
  registerCleanupFunction(() => SpecialPowers.popPrefEnv());

  ChromeUtils.clearResourceCache();
  Services.cache2.clear();

  const resetResponse = await fetch(TEST_SCRIPT_URL + "?reset");
  is(await resetResponse.text(), "reset", "Server state should be reset");

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_URL,
  });

  is(await getCounter(tab, type), 0, "counter should be 0 for the first load.");

  is(
    await reloadAndGetCounter(tab, type),
    0,
    "cache should be used for subsequent load."
  );

  ChromeUtils.clearResourceCache();
  Services.cache2.clear();
  is(
    await reloadAndGetCounter(tab, type),
    1,
    "request should reach the server after removing all cache."
  );
  is(
    await reloadAndGetCounter(tab, type),
    1,
    "cache should be used for subsequent load."
  );

  ChromeUtils.clearResourceCache();
  is(await reloadAndGetCounter(tab, type), 1, "network cache should be used.");

  Services.cache2.clear();
  if (!useNavigationCache) {
    is(
      await reloadAndGetCounter(tab, type),
      2,
      "request should reach the server after network cache is cleared."
    );
  } else {
    // The above reload loads from the network cache, and the JS cache is
    // re-created.

    is(await reloadAndGetCounter(tab, type), 1, "JS cache should be used.");
  }

  ChromeUtils.clearResourceCache();
  if (!useNavigationCache) {
    is(
      await reloadAndGetCounter(tab, type),
      2,
      "network cache should be used."
    );
  } else {
    // The above reload loads from the JS cache.
    // Currently, network cache is not re-created from the JS cache.
    is(
      await reloadAndGetCounter(tab, type),
      2,
      "request should reach the server after both cache is cleared."
    );
  }

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_scriptWithoutNavigationCache() {
  await doTest(false, "script");
});

add_task(async function test_scriptWithNavigationCache() {
  await doTest(true, "script");
});

add_task(async function test_moduleTopLevelWithoutNavigationCache() {
  await doTest(false, "module-top-level");
});

add_task(async function test_moduleTopLevelWithNavigationCache() {
  await doTest(true, "module-top-level");
});

add_task(async function test_moduleStaticWithoutNavigationCache() {
  await doTest(false, "module-static");
});

add_task(async function test_moduleStaticWithNavigationCache() {
  await doTest(true, "module-static");
});

add_task(async function test_moduleDynamicWithoutNavigationCache() {
  await doTest(false, "module-dynamic");
});

add_task(async function test_moduleDynamicWithNavigationCache() {
  await doTest(true, "module-dynamic");
});
