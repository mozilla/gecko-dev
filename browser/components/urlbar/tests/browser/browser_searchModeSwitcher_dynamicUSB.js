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

add_task(async function test_usb_visibility_by_tab() {
  for (const shiftKey of [false, true]) {
    info(`Test for shiftKey:${shiftKey}`);

    info("Open pageproxystate valid page");
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
    Assert.equal(gURLBar.getAttribute("pageproxystate"), "valid");
    info("Ensure to move focus to browser element");
    await focusOnBrowserElement();
    await assertUSBVisibility(false);

    info("Focus on urlabr");
    EventUtils.synthesizeKey("l", { accelKey: true });
    await UrlbarTestUtils.promiseSearchComplete(window);
    await assertUSBVisibility(true);
    Assert.equal(document.activeElement.id, "urlbar-input");
    Assert.ok(gURLBar.view.isOpen);
    Assert.ok(!gURLBar.view.selectedElement);

    info("Move the focus until urlbar has it again");
    let ok = false;
    for (let i = 0; i < 10; i++) {
      let previousActiveElement = document.activeElement;
      EventUtils.synthesizeKey("KEY_Tab", { shiftKey });
      await BrowserTestUtils.waitForCondition(
        () => previousActiveElement != document.activeElement,
        "Wait until the active element is changed"
      );
      await assertUSBVisibility(!!document.activeElement.closest("#nav-bar"));
      Assert.ok(true, "USB visibility is correct");

      ok = document.activeElement.id == "urlbar-input";
      if (ok) {
        break;
      }
    }
    Assert.ok(ok, "Focus was moved back to urlabr via other components");
    await BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function test_usb_visibility_by_mouse() {
  info("Open pageproxystate valid page");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );
  Assert.equal(gURLBar.getAttribute("pageproxystate"), "valid");
  info("Ensure to move focus to browser element");
  await focusOnBrowserElement();
  await assertUSBVisibility(false);

  info("Click on urlbar");
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  await assertUSBVisibility(true);

  info("Click on browser element");
  await focusOnBrowserElement();
  await assertUSBVisibility(false);

  info("Click on urlbar again");
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
  await assertUSBVisibility(true);

  info("Simulate to lost focus from the current window");
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  await assertUSBVisibility(false);

  await BrowserTestUtils.closeWindow(newWin);
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_usb_visibility_by_mouse_drag() {
  info("Open pageproxystate valid page");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );
  Assert.equal(gURLBar.getAttribute("pageproxystate"), "valid");
  info("Ensure to move focus to browser element");
  await focusOnBrowserElement();
  await assertUSBVisibility(false);

  info("Mouse down on urlbar");
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, { type: "mousedown" });
  await assertUSBVisibility(false);

  info("Hold mouse down 1sec");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 1000));
  await assertUSBVisibility(false);

  info("Mouse up on urlbar");
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, { type: "mouseup" });
  await assertUSBVisibility(true);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_usb_visibility_during_popup() {
  info("Open pageproxystate valid page");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );
  Assert.equal(gURLBar.getAttribute("pageproxystate"), "valid");
  info("Ensure to move focus to browser element");
  await focusOnBrowserElement();
  await assertUSBVisibility(false);

  info("Focus on urlabr");
  EventUtils.synthesizeKey("l", { accelKey: true });
  await UrlbarTestUtils.promiseSearchComplete(window);
  await assertUSBVisibility(true);

  info("Open popup");
  await UrlbarTestUtils.openSearchModeSwitcher(window);
  await assertUSBVisibility(true);

  info("Close popup");
  let popupHidden = UrlbarTestUtils.searchModeSwitcherPopupClosed(window);
  EventUtils.synthesizeKey("KEY_Escape");
  await popupHidden;
  await assertUSBVisibility(true);

  BrowserTestUtils.removeTab(tab);
});

async function assertUSBVisibility(expected) {
  let switcher = document.getElementById("urlbar-searchmode-switcher");
  await BrowserTestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(switcher) == expected,
    `Wait until USB visibility will be changed to ${expected}`
  );
  Assert.ok(true, "USB visibility is correct");
}

async function focusOnBrowserElement() {
  // We intentionally turn off this a11y check, because the following click is
  // purposefully targeting a non-interactive element.
  AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
  EventUtils.synthesizeMouseAtCenter(document.getElementById("browser"), {});
  AccessibilityUtils.resetEnv();
  await BrowserTestUtils.waitForCondition(() =>
    document.activeElement.closest("#browser")
  );
}
