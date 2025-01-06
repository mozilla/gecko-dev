"use strict";

// Test the offline mode in RDM

const TEST_URL = "https://example.net/document-builder.sjs?html=offline";

addRDMTask(TEST_URL, async function ({ browser, ui }) {
  // switch to offline mode
  await selectNetworkThrottling(ui, "Offline");

  // reload w/o cache
  browser.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
  await BrowserTestUtils.browserLoaded(browser, false, TEST_URL, true);

  // check page is offline
  await assertPageIsOffline(TEST_URL);

  // switch back to online mode
  await selectNetworkThrottling(ui, "No Throttling");

  // reload
  browser.reload();
  await BrowserTestUtils.browserLoaded(browser, false, TEST_URL, false);

  // check page is online
  await assertPageIsOnline(TEST_URL);
});

function assertPageIsOnline(url) {
  return SpecialPowers.spawn(
    gBrowser.selectedTab.linkedBrowser,
    [url],
    function (uri) {
      is(content.document.documentURI, uri, `Document URI is actual page.`);
      is(content.location.href, uri, "Docshell URI is the original URI.");
    }
  );
}

function assertPageIsOffline(url) {
  return SpecialPowers.spawn(
    gBrowser.selectedTab.linkedBrowser,
    [url],
    function (uri) {
      is(
        content.document.documentURI.substring(0, 27),
        "about:neterror?e=netOffline",
        "Document URI is the error page."
      );

      is(content.location.href, uri, "Docshell URI is the original URI.");
    }
  );
}
