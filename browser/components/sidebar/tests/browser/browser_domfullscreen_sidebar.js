/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
const { DOMFullscreenTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/DOMFullscreenTestUtils.sys.mjs"
);
let win;

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({ set: [["sidebar.revamp", true]] });
  DOMFullscreenTestUtils.init(this, window);
  win = await BrowserTestUtils.openNewBrowserWindow();
  await waitForBrowserWindowActive(win);
});

registerCleanupFunction(async () => {
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_dom_fullscreen() {
  // ensure the sidebar becomes hidden in dom fullscreen
  const url = "https://example.com/";
  const sidebarLauncher = win.document.getElementById("sidebar-main");

  ok(
    BrowserTestUtils.isVisible(sidebarLauncher),
    "Sidebar launcher is initially visible"
  );

  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url },
    async browser => {
      // the newly opened tab should have focus
      await DOMFullscreenTestUtils.changeFullscreen(browser, true);

      is(win.document.fullscreenElement, browser, "Entered DOM fullscreen");
      ok(
        BrowserTestUtils.isHidden(sidebarLauncher),
        "Sidebar launcher is hidden in DOMFullscreen"
      );

      await DOMFullscreenTestUtils.changeFullscreen(browser, false);
      ok(
        BrowserTestUtils.isVisible(sidebarLauncher),
        "Sidebar launcher becomes visible when we exit DOMFullscreen"
      );
    }
  );
});
