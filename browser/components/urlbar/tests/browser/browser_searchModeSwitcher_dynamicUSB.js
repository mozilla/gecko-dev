/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = `${TEST_BASE_URL}has-a-link.html`;

ChromeUtils.defineESModuleGetters(this, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });
});

add_task(async function test_usb_visibility_by_pageproxystate() {
  info("Open pageproxystate valid page");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );
  await assertUSBVisibility(false);

  info("Click on browser element");
  await clickOnBrowserElement();
  await assertUSBVisibility(false);

  info("Click on urlbar");
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  await assertUSBVisibility(false);

  info("Start to edit");
  EventUtils.synthesizeKey("a");
  await assertUSBVisibility(true);

  info("Click on browser element");
  await clickOnBrowserElement();
  await assertUSBVisibility(true);

  BrowserTestUtils.removeTab(tab);
});

async function assertUSBVisibility(expected) {
  let switcher = document.getElementById("urlbar-searchmode-switcher");
  await BrowserTestUtils.waitForCondition(() => {
    // If Unified Search Button is displayed as off-screen, the position should
    // be 'fixed'.
    let isVisible = window.getComputedStyle(switcher).position != "fixed";
    return isVisible == expected;
  }, `Wait until USB visibility will be changed to ${expected}`);
  Assert.ok(true, "USB visibility is correct");
  Assert.equal(
    gURLBar.getAttribute("pageproxystate"),
    expected ? "invalid" : "valid"
  );
}

async function clickOnBrowserElement() {
  // We intentionally turn off this a11y check, because the following click is
  // purposefully targeting a non-interactive element.
  AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
  EventUtils.synthesizeMouseAtCenter(document.getElementById("browser"), {});
  AccessibilityUtils.resetEnv();
  await BrowserTestUtils.waitForCondition(() =>
    document.activeElement.closest("#browser")
  );
}
