/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  URILoadingHelper: "resource:///modules/URILoadingHelper.sys.mjs",
});

const URL = "https://example.com/";

// Open a taskbar tab window, and then call openLinkIn()
// to trigger the URILoadingHelper code. The webpage should
// open in the regular Firefox window instead of the taskbar tab
// window after Bug 1945000.
add_task(async function testUriNewTab() {
  let win = await openTaskbarTabWindow();

  URILoadingHelper.openTrustedLinkIn(win, URL, "tab");
  is(
    window.gBrowser.openTabs.length,
    2,
    "The new tab should've opened in the regular Firefox window"
  );
  BrowserTestUtils.removeTab(window.gBrowser.selectedTab);

  URILoadingHelper.openTrustedLinkIn(win, URL, "tabshifted");
  is(
    window.gBrowser.openTabs.length,
    2,
    "The new tab should've opened in the regular Firefox window"
  );
  BrowserTestUtils.removeTab(window.gBrowser.selectedTab);

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testToolbarCustomizer() {
  let win = await openTaskbarTabWindow();
  win.gCustomizeMode.enter();

  is(
    window.gBrowser.openTabs.length,
    2,
    "The toolbar customizer tab should've opened in the regular Firefox window"
  );

  BrowserTestUtils.removeTab(window.gBrowser.selectedTab);
  await BrowserTestUtils.closeWindow(win);
});
