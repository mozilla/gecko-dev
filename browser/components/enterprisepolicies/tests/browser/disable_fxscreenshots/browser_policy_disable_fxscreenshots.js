/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ScreenshotsUtils: "resource:///modules/ScreenshotsUtils.sys.mjs",
});

const PREF_DISABLE_FX_SCREENSHOTS = "screenshots.browser.component.enabled";

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

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["test.wait300msAfterTabSwitch", true]],
  });
});

add_task(async function test_disable_firefox_screenshots() {
  // Dynamically toggling the PREF_DISABLE_FX_SCREENSHOTS is very finicky, because
  // that pref is being watched, and it makes the Firefox Screenshots component
  // to start or stop, causing intermittency.
  //
  // The screenshots component is enabled by default so we let the policy
  // engine be responsible for disabling Firefox Screenshots in this case.

  ok(
    !Services.prefs.getBoolPref(PREF_DISABLE_FX_SCREENSHOTS),
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
