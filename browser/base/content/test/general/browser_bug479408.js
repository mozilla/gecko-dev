/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(this, {
  OpenSearchManager:
    "moz-src:///browser/components/search/OpenSearchManager.sys.mjs",
});

function test() {
  waitForExplicitFinish();
  let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(
    gBrowser,
    "http://mochi.test:8888/browser/browser/base/content/test/general/browser_bug479408_sample.html"
  ));

  BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser,
    "DOMLinkAdded",
    true
  ).then(() => {
    executeSoon(function () {
      Assert.equal(
        OpenSearchManager.getEngines(tab.linkedBrowser).length,
        0,
        "the subframe's search engine wasn't detected"
      );

      gBrowser.removeTab(tab);
      finish();
    });
  });
}
