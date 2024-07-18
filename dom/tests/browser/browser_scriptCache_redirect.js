const TEST_URL = "https://example.com/browser/dom/tests/browser/dummy.html";
const SCRIPT_NAME = "redirect_server.sjs";
const TEST_SCRIPT_URL =
  "https://example.com/browser/dom/tests/browser/" + SCRIPT_NAME;

function getCounter(tab, query) {
  const browser = tab.linkedBrowser;
  const scriptPath = SCRIPT_NAME + query;
  return SpecialPowers.spawn(browser, [scriptPath], async scriptPath => {
    const { promise, resolve } = Promise.withResolvers();

    const script = content.document.createElement("script");
    script.src = scriptPath;
    script.addEventListener("load", resolve);
    content.document.body.appendChild(script);

    await promise;

    return parseInt(content.document.body.getAttribute("counter"));
  });
}

async function reloadAndGetCounter(tab, query) {
  await BrowserTestUtils.reloadTab(tab);

  return getCounter(tab, query);
}

function clearAllCache() {
  return new Promise(function (resolve) {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_ALL_CACHES,
      resolve
    );
  });
}

add_task(async function test_redirectCache() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.script_loader.navigation_cache", true]],
  });
  registerCleanupFunction(() => SpecialPowers.popPrefEnv());

  const tests = [
    {
      query: "?redirect=cacheable&script=cacheable",
      cachedCounter: true,
      log: [",redirect=cacheable&script=cacheable", ",script=cacheable"].join(
        ""
      ),
    },
    {
      query: "?redirect=cacheable&script=not-cacheable",
      cachedCounter: false,
      log: [
        ",redirect=cacheable&script=not-cacheable",
        ",script=not-cacheable",
        ",script=not-cacheable",
      ].join(""),
    },
    {
      query: "?redirect=not-cacheable&script=cacheable",
      cachedCounter: true,
      log: [
        ",redirect=not-cacheable&script=cacheable",
        ",script=cacheable",
        ",redirect=not-cacheable&script=cacheable",
      ].join(""),
    },
    {
      query: "?redirect=not-cacheable&script=not-cacheable",
      cachedCounter: false,
      log: [
        ",redirect=not-cacheable&script=not-cacheable",
        ",script=not-cacheable",
        ",redirect=not-cacheable&script=not-cacheable",
        ",script=not-cacheable",
      ].join(""),
    },
  ];

  for (const { query, cachedCounter, log } of tests) {
    await clearAllCache();

    const resetResponse = await fetch(TEST_SCRIPT_URL + "?reset");
    is(await resetResponse.text(), "reset", "Server state should be reset");

    const tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: TEST_URL,
    });

    is(
      await getCounter(tab, query),
      0,
      "counter should be 0 for the first load."
    );

    const counter = await reloadAndGetCounter(tab, query);
    if (cachedCounter) {
      is(counter, 0, "cache should be used for " + query);
    } else {
      is(counter, 1, "cache should not be used for " + query);
    }

    const logResponse = await fetch(TEST_SCRIPT_URL + "?log");
    is(await logResponse.text(), log, "Log should match");

    BrowserTestUtils.removeTab(tab);
  }
});
