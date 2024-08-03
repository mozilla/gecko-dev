/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that shortcuts aren't shown by default
 */
add_task(async function test_no_shortcuts() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.chat.provider", "http://localhost:8080"]],
  });
  await BrowserTestUtils.withNewTab("data:text/plain,hi", async browser => {
    browser.focus();
    goDoCommand("cmd_selectAll");
    Assert.ok(
      !document.querySelector(".content-shortcuts"),
      "No shortcuts found"
    );
  });
});

/**
 * Check that shortcuts get shown on selection and open popup and sidebar
 */
add_task(async function test_show_shortcuts() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.ml.chat.shortcuts", true],
      ["browser.ml.chat.shortcutsDebounce", 0],
    ],
  });
  await BrowserTestUtils.withNewTab("data:text/plain,hi", async browser => {
    await SimpleTest.promiseFocus(browser);
    goDoCommand("cmd_selectAll");
    const shortcuts = await TestUtils.waitForCondition(() =>
      document.querySelector(".content-shortcuts")
    );
    Assert.ok(shortcuts, "Shortcuts added on select");

    const popup = document.getElementById("ask-chat-shortcuts");
    Assert.equal(popup.state, "closed", "Popup is closed");

    EventUtils.sendMouseEvent({ type: "mouseover" }, shortcuts);
    await BrowserTestUtils.waitForEvent(popup, "popupshown");
    Assert.equal(popup.state, "open", "Popup is open");

    Assert.ok(!SidebarController.isOpen, "Sidebar is closed");
    popup.querySelector("toolbarbutton").click();
    Assert.ok(SidebarController.isOpen, "Chat opened sidebar");

    SidebarController.hide();
  });
});
