/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
});

const PREF_DISABLE_FX_SCREENSHOTS = "extensions.screenshots.disabled";

async function checkScreenshots(shouldBeEnabled) {
  let menu = document.getElementById("contentAreaContextMenu");
  let popupshown = BrowserTestUtils.waitForPopupEvent(menu, "shown");
  EventUtils.synthesizeMouseAtCenter(document.body, {
    type: "contextmenu",
  });
  await popupshown;
  Assert.equal(menu.state, "open", "Context menu is open");

  is(
    menu.querySelector("#context-take-screenshot").hidden,
    !shouldBeEnabled,
    `Screenshots context menu item should be ${shouldBeEnabled ? "shown" : "hidden"}`
  );

  let popuphidden = BrowserTestUtils.waitForPopupEvent(menu, "hidden");
  menu.hidePopup();
  await popuphidden;
}

add_task(async function test_disable_firefox_screenshots() {
  // Dynamically toggling the PREF_DISABLE_FX_SCREENSHOTS is very finicky, because
  // that pref is being watched, and it makes the Firefox Screenshots component
  // to start or stop, causing intermittency.
  //
  // Firefox Screenshots is disabled by default on tests (in
  // testing/profiles/common/user.js). What we do here to test this policy is to enable
  // it on this specific test folder (through browser.ini) and then we let the policy
  // engine be responsible for disabling Firefox Screenshots in this case.

  is(
    Services.prefs.getBoolPref(PREF_DISABLE_FX_SCREENSHOTS),
    true,
    "Screenshots pref is disabled"
  );

  await BrowserTestUtils.waitForCondition(
    () => !ScreenshotsUtils.initialized,
    "Wait for the screenshot component to be uninitialized"
  );
  ok(
    !ScreenshotsUtils.initialized,
    "The screenshot component is uninitialized"
  );

  await BrowserTestUtils.withNewTab("data:text/html,Test", async function () {
    await checkScreenshots(false);
  });
});
