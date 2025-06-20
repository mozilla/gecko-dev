const { UrlClassifierTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlClassifierTestUtils.sys.mjs"
);

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const baseURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

// Tests that a public->private fetch load initiated from
// a tracking script will fail.
add_task(async function test_tracker_initiated_lna_fetch() {
  let server = new HttpServer();
  server.start(21555);
  registerCleanupFunction(async () => {
    await server.stop();
  });
  server.registerPathHandler("/", (request, response) => {
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`hello`);
  });

  registerCleanupFunction(UrlClassifierTestUtils.cleanupTestTrackers);
  await UrlClassifierTestUtils.addTestTrackers();

  // Open the test page.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    baseURL + "page_with_trackers.html"
  );

  let result = await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await new Promise(resolve => content.setTimeout(resolve, 1000));
    return content.wrappedJSObject.result;
  });

  Assert.equal(result, "FAIL");
  gBrowser.removeTab(tab);
});
