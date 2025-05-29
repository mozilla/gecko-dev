/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function getPageURL(domain) {
  // The iframe in file_browsingContext_iframeWebProgress.html is hardcoded to
  // point to example.com
  return (
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      `https://${domain}`
    ) + "file_browsingContext_iframeWebProgress.html"
  );
}

const SAME_ORIGIN_IFRAME_PAGE = getPageURL("example.com");
const CROSS_ORIGIN_IFRAME_PAGE = getPageURL("example.org");

add_task(async function testReloadCrossOriginIframePage() {
  await testReloadPageWithIframe(CROSS_ORIGIN_IFRAME_PAGE);
});

add_task(async function testReloadSameOriginIframePage() {
  await testReloadPageWithIframe(SAME_ORIGIN_IFRAME_PAGE);
});

async function testReloadPageWithIframe(url) {
  const tab = BrowserTestUtils.addTab(gBrowser, url);

  const browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser, false, url);

  const onStateChangeEvents = [];
  const listener = {
    QueryInterface: ChromeUtils.generateQI([
      "nsIWebProgressListener",
      "nsISupportsWeakReference",
    ]),
    onStateChange(webProgress, request, flags) {
      if (flags & Ci.nsIWebProgressListener.STATE_START) {
        onStateChangeEvents.push(
          request.QueryInterface(Ci.nsIChannel).originalURI
        );
      }
    },
  };
  browser.browsingContext.webProgress.addProgressListener(
    listener,
    Ci.nsIWebProgress.NOTIFY_STATE_WINDOW
  );
  BrowserTestUtils.reloadTab(tab);

  await BrowserTestUtils.waitForCondition(
    () => onStateChangeEvents.length == 2
  );
  is(onStateChangeEvents.length, 2);

  gBrowser.removeTab(tab);
}
