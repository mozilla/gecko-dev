/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This test opens a private browsing window, then opens a content page in it
 * that loads an svg image that contains an image to an external protocol.
 * This tests that we don't hit an assert in this situation.
 */

add_task(async function test() {
  function httpURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  let tab = (win.gBrowser.selectedTab = BrowserTestUtils.addTab(
    win.gBrowser,
    "about:blank"
  ));

  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  const pageUrl = httpURL("helper1899180.html");

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, pageUrl);

  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await new Promise(resolve => {
    waitForFocus(resolve, win);
  });

  // do a couple rafs here to ensure its loaded and displayed
  await new Promise(r => requestAnimationFrame(r));
  await new Promise(r => requestAnimationFrame(r));

  await BrowserTestUtils.closeWindow(win);

  win = null;
  tab = null;

  ok(true, "we got here and didn't crash/assert");
});
